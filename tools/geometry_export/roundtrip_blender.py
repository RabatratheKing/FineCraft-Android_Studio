"""Import exported FineCraft prototype assets into a separate Blender scene."""

from __future__ import annotations

import json
import math
import struct
from collections import defaultdict
from pathlib import Path

import bpy

import export_blender


def _read_asset(metadata_path):
    metadata_path = Path(metadata_path)
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    binary = (metadata_path.parent / metadata["geometry"]["binary_file"]).read_bytes()
    magic, version, header_bytes, count = struct.unpack_from("<4sHHI", binary)
    if magic != export_blender.BINARY_MAGIC or version != export_blender.BINARY_VERSION or header_bytes != export_blender.BINARY_HEADER.size:
        raise ValueError(f"unsupported geometry binary header in {metadata_path}")
    if count != len(metadata["geometry"]["components"]):
        raise ValueError(f"component count mismatch in {metadata_path}")
    decoded = []
    for component in metadata["geometry"]["components"]:
        vb = component["vertex_buffer"]; ib = component["index_buffer"]
        vertices = [struct.unpack_from("<6f", binary, vb["byte_offset"] + i * 24) for i in range(component["vertex_count"])]
        indices = [struct.unpack_from("<I", binary, ib["byte_offset"] + i * 4)[0] for i in range(component["index_count"])]
        if any(index >= len(vertices) for index in indices):
            raise ValueError(f"out-of-range index in {metadata_path}:{component['name']}")
        decoded.append((component, vertices, indices))
    return metadata, decoded


def _bounds(points):
    points = list(points)
    if not points:
        return None
    return [[min(p[i] for p in points) for i in range(3)], [max(p[i] for p in points) for i in range(3)]]


def _max_delta(left, right):
    if len(left) != len(right):
        return float("inf")
    return max((abs(float(a)-float(b)) for x,y in zip(left,right) for a,b in zip(x,y)), default=0.0)


def _make_box_mesh(box):
    x0,y0,z0,x1,y1,z1 = box
    corners_mc = [(x,y,z) for x in (x0,x1) for y in (y0,y1) for z in (z0,z1)]
    corners_bl = [export_blender.minecraft_to_blender(p) for p in corners_mc]
    # Cube face winding is not consumed by this bounds round-trip assertion.
    faces = [(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)]
    mesh = bpy.data.meshes.new("RT_CollisionMesh")
    mesh.from_pydata(corners_bl, [], faces)
    mesh.update()
    return mesh


def run_roundtrip(asset_dir, report_path, scene_name=export_blender.DEFAULT_SCENE, tolerance=export_blender.COORD_TOLERANCE):
    source_scene = bpy.data.scenes.get(scene_name)
    if source_scene is None:
        raise ValueError(f"source scene {scene_name!r} not found")
    asset_dir = Path(asset_dir)
    metadata_paths = sorted(asset_dir.glob("*.json"))
    if not metadata_paths:
        raise ValueError(f"no exported metadata JSON files in {asset_dir}")

    wanted = {json.loads(p.read_text(encoding="utf-8"))["block_id"] for p in metadata_paths}
    groups = export_blender._prototype_groups(source_scene, wanted)
    scene_before_count = len(source_scene.objects)
    roundtrip_scene = bpy.data.scenes.new("FineCraft_GeometryExport_RoundTrip")
    report = {
        "status": "pass",
        "source_scene": source_scene.name,
        "temporary_scene": roundtrip_scene.name,
        "source_scene_object_count_before": scene_before_count,
        "tolerance": tolerance,
        "assets": [],
    }

    try:
        for metadata_path in metadata_paths:
            metadata, decoded = _read_asset(metadata_path)
            key_candidates = [key for key in groups if key[0] == metadata["block_id"] and key[1] == metadata["prototype_id"]]
            if len(key_candidates) != 1:
                raise ValueError(f"could not uniquely find source prototype for {metadata['block_id']}")
            source_visuals = groups[key_candidates[0]]["visual"]
            anchor_obj, anchor_world, anchor_cell = export_blender._prototype_anchor(source_scene, metadata["block_id"], metadata["prototype_id"], source_visuals)

            comparison = {"block_id": metadata["block_id"], "state_ids": [s["state_id"] for s in metadata["states"]], "components": [], "collision": []}
            imported_by_state = defaultdict(list)
            original_positions = []
            imported_positions = []
            for component, packed_vertices, indices in decoded:
                source_obj = next((o for o in source_visuals if int(o.get("state_id", o.get("fc_state_id", -1))) == component["state_id"] and str(o.get("fc_part", o.name)) == component["name"]), None)
                if source_obj is None:
                    raise ValueError(f"source mesh missing for {metadata['block_id']}:{component['name']} state {component['state_id']}")
                source_vertices, source_indices, _ = export_blender._triangulate_component(source_obj, anchor_world)
                if len(source_indices) != len(indices) or len(source_vertices) != len(packed_vertices):
                    raise ValueError(f"triangle vertex count mismatch for {metadata['block_id']}:{component['name']}")
                packed_positions = [tuple(v[:3]) for v in packed_vertices]
                position_delta = max((abs(source_vertices[i][axis]-packed_positions[i][axis]) for i in range(len(packed_positions)) for axis in range(3)), default=0.0)
                packed_normals = [tuple(v[3:]) for v in packed_vertices]
                normal_delta = max((abs(source_vertices[i][axis+3]-packed_normals[i][axis]) for i in range(len(packed_normals)) for axis in range(3)), default=0.0)

                blender_positions = [export_blender.minecraft_to_blender(v[:3]) for v in packed_vertices]
                faces = [tuple(indices[i:i+3]) for i in range(0,len(indices),3)]
                mesh = bpy.data.meshes.new("RT_" + component["name"])
                mesh.from_pydata(blender_positions, [], faces)
                mesh.update()
                obj = bpy.data.objects.new(f"RT_{metadata['block_id']}_{component['state_id']}_{component['name']}", mesh)
                obj["minecraft_id"] = metadata["block_id"]
                obj["state_id"] = component["state_id"]
                obj["component_name"] = component["name"]
                roundtrip_scene.collection.objects.link(obj)
                imported_by_state[component["state_id"]].append(obj)

                imported_canonical = [export_blender.blender_to_minecraft(v.co) for v in mesh.vertices]
                imported_positions.extend(imported_canonical)
                original_positions.extend(packed_positions)
                bbox_delta = _max_delta(_bounds(source_vertices[i][:3] for i in range(len(source_vertices))), _bounds(imported_canonical))
                face_normal_delta = 0.0
                for tri_index, poly in enumerate(mesh.polygons):
                    imported_normal = export_blender.blender_to_minecraft(poly.normal)
                    packed_normal = packed_normals[tri_index*3]
                    face_normal_delta = max(face_normal_delta, *(abs(imported_normal[a]-packed_normal[a]) for a in range(3)))
                component_pass = position_delta <= tolerance and normal_delta <= 1e-5 and face_normal_delta <= 1e-5 and bbox_delta <= tolerance and len(faces) == component["triangle_count"]
                comparison["components"].append({
                    "name": component["name"], "state_id": component["state_id"],
                    "triangle_count": len(faces), "vertex_position_max_delta": position_delta,
                    "normal_max_delta": max(normal_delta, face_normal_delta), "bounds_max_delta": bbox_delta,
                    "status": "pass" if component_pass else "fail",
                })

            # Validate that the exported collision boxes reconstruct as separate
            # islands and preserve their dimensions and prototype-local offsets.
            expected_collision_total = 0
            for state in metadata["states"]:
                boxes = state["collision_boxes"]
                expected_collision_total += len(boxes)
                recreated = []
                for box_index, box in enumerate(boxes):
                    mesh = _make_box_mesh(box)
                    obj = bpy.data.objects.new(f"RT_COL_{metadata['block_id']}_{state['state_id']}_{box_index}", mesh)
                    roundtrip_scene.collection.objects.link(obj)
                    points = [export_blender.blender_to_minecraft(v.co) for v in mesh.vertices]
                    bounds = _bounds(points)
                    actual_box = [*bounds[0], *bounds[1]]
                    recreated.append(actual_box)
                quant = lambda arr: sorted(tuple(round(float(x), 6) for x in box) for box in arr)
                coll_pass = quant(recreated) == quant(boxes)
                comparison["collision"].append({"state_id": state["state_id"], "source_shape_id": state["source_shape_id"], "box_count": len(boxes), "status": "pass" if coll_pass else "fail"})

            source_boxes = defaultdict(list)
            for collider in groups[key_candidates[0]]["collision"]:
                sid = int(collider.get("state_id", collider.get("fc_state_id", -1)))
                source_boxes[sid].extend(export_blender._collision_boxes(collider, anchor_world))
            for state in metadata["states"]:
                quant = lambda arr: sorted(tuple(round(float(x), 6) for x in box) for box in arr)
                if quant(source_boxes[state["state_id"]]) != quant(state["collision_boxes"]):
                    comparison["collision"].append({"state_id": state["state_id"], "status": "fail", "reason": "export differs from original collision proxy"})

            original_bounds = _bounds(original_positions)
            reimport_bounds = _bounds(imported_positions)
            aggregate_bounds_delta = _max_delta(original_bounds, reimport_bounds)
            component_count = len(comparison["components"])
            expected_component_count = len(metadata["geometry"]["components"])
            comparison.update({
                "component_count": component_count,
                "triangle_count": sum(c["triangle_count"] for c in comparison["components"]),
                "collision_box_count": expected_collision_total,
                "source_bounds": original_bounds,
                "reimport_bounds": reimport_bounds,
                "aggregate_bounds_max_delta": aggregate_bounds_delta,
            })
            passed = (
                component_count == expected_component_count and aggregate_bounds_delta <= tolerance and
                all(c["status"] == "pass" for c in comparison["components"]) and
                all(c["status"] == "pass" for c in comparison["collision"])
            )
            comparison["status"] = "pass" if passed else "fail"
            report["assets"].append(comparison)

        report["source_scene_object_count_after"] = len(source_scene.objects)
        if report["source_scene_object_count_after"] != scene_before_count:
            report["status"] = "fail"
        if any(asset["status"] != "pass" for asset in report["assets"]):
            report["status"] = "fail"
        report_path = Path(report_path)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return report
    except Exception:
        report["status"] = "fail"
        report["source_scene_object_count_after"] = len(source_scene.objects)
        raise


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-dir", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args()
    print(json.dumps(run_roundtrip(args.asset_dir, args.report), indent=2))
