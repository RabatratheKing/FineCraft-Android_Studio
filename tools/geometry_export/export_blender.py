"""Export FineCraft prototype meshes from a Blender scene.

Run inside Blender 5.2+ with:
  blender --background scene.blend --python export_blender.py -- \
      --output-dir app/src/main/assets/geometry/prototypes \
      --blocks minecraft:stone,minecraft:oak_stairs

Without --blocks (or with --all-prototypes), every logical prototype found in
the selected FineCraft scene is exported. No source object is modified.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

try:
    import bpy
except ImportError:  # Keeps pure helpers importable in normal Python tests.
    bpy = None


SCHEMA_VERSION = 1
BINARY_VERSION = 1
BINARY_MAGIC = b"FCGB"
BINARY_HEADER = struct.Struct("<4sHHI")  # magic, version, header bytes, component count
VERTEX = struct.Struct("<6f")  # position.xyz, normal.xyz
INDEX = struct.Struct("<I")
COORD_TOLERANCE = 1e-6
DEFAULT_SCENE = "FineCraft_GeometryLab"


def blender_to_minecraft(vector):
    """Convert Blender XYZ vectors to Minecraft XYZ: (x, z, -y)."""
    return (float(vector[0]), float(vector[2]), -float(vector[1]))


def minecraft_to_blender(vector):
    """Convert Minecraft XYZ vectors to Blender XYZ: (x, -z, y)."""
    return (float(vector[0]), -float(vector[2]), float(vector[1]))


def round_trip(vector):
    return minecraft_to_blender(blender_to_minecraft(vector))


def _json_property(obj, key, default=None):
    value = obj.get(key, default)
    if isinstance(value, str):
        try:
            return json.loads(value)
        except (json.JSONDecodeError, TypeError):
            return value
    return value


def _as_vec3(matrix_result):
    return (float(matrix_result[0]), float(matrix_result[1]), float(matrix_result[2]))


def _sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def _determinant3(matrix):
    return float(matrix.to_3x3().determinant())


def _prototype_groups(scene, selected_block_ids=None):
    groups = defaultdict(lambda: {"visual": [], "collision": []})
    selected = set(selected_block_ids or [])
    for obj in scene.objects:
        role = obj.get("fc_role")
        if role not in {"visual", "collision"} or obj.type != "MESH":
            continue
        block_id = obj.get("minecraft_id", obj.get("fc_block_id"))
        if not block_id or (selected and block_id not in selected):
            continue
        proto_id = obj.get("fc_prototype")
        # Collision objects from Phase 2 predate fc_prototype. Match them to
        # their linked visual by block/state metadata, then assign the visual's
        # logical prototype identifier below.
        if role == "visual" and proto_id is not None:
            groups[(str(block_id), int(proto_id))]["visual"].append(obj)
    for obj in scene.objects:
        if obj.get("fc_role") != "collision" or obj.type != "MESH":
            continue
        block_id = str(obj.get("minecraft_id", obj.get("fc_block_id", "")))
        if not block_id or (selected and block_id not in selected):
            continue
        state_id = int(obj.get("state_id", obj.get("fc_state_id", -1)))
        candidate_keys = [k for k, g in groups.items() if k[0] == block_id and any(
            int(v.get("state_id", v.get("fc_state_id", -2))) == state_id for v in g["visual"]
        )]
        if len(candidate_keys) == 1:
            groups[candidate_keys[0]]["collision"].append(obj)
    return groups


def _choose_anchor(objects):
    """Use lower/foot state for multipart blocks; otherwise the lowest cell."""
    preferred = []
    for obj in objects:
        props = _json_property(obj, "state_properties", obj.get("fc_state_properties", "{}")) or {}
        if props.get("half") == "lower" or props.get("part") == "foot":
            preferred.append(obj)
    candidates = preferred or list(objects)
    return min(candidates, key=lambda o: (
        int(json.loads(o.get("fc_mc_cell", "[0,0,0]"))[1]),
        int(o.get("state_id", o.get("fc_state_id", 0))),
        o.name,
    ))


def _anchor_world(anchor_obj):
    return _as_vec3(anchor_obj.matrix_world.translation)


def _canonical_position(obj, vertex, anchor_world):
    world = obj.matrix_world @ vertex.co
    delta = _sub(_as_vec3(world), anchor_world)
    return blender_to_minecraft(delta)


def _canonical_normal(obj, normal):
    # The matrix normal transform is inverse-transpose; normalize after basis
    # conversion. Reflection is handled by reversing each triangle's winding.
    transformed = obj.matrix_world.to_3x3().inverted().transposed() @ normal
    converted = blender_to_minecraft(transformed)
    length = math.sqrt(sum(c * c for c in converted))
    if length <= 1e-12:
        raise ValueError(f"zero-length normal on {obj.name}")
    return tuple(c / length for c in converted)


def _triangulate_component(obj, anchor_world):
    """Return de-duplicated triangle vertices and sequential indices.

    Evaluated geometry is copied out of Blender. The source object and mesh are
    never edited. Blender's calc_loop_triangles gives deterministic face fans
    for this prototype set and preserves per-face polygon normals.
    """
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated_obj = obj.evaluated_get(depsgraph)
    mesh = evaluated_obj.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
    try:
        mesh.calc_loop_triangles()
        determinant = _determinant3(obj.matrix_world)
        vertices = []
        indices = []
        oriented_triangles = set()
        degenerate = 0
        duplicate = 0
        for tri in mesh.loop_triangles:
            poly = mesh.polygons[tri.polygon_index]
            tri_indices = list(tri.vertices)
            if determinant < 0.0:
                tri_indices[1], tri_indices[2] = tri_indices[2], tri_indices[1]
            points = [_canonical_position(obj, mesh.vertices[i], anchor_world) for i in tri_indices]
            a, b, c = points
            ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            cross = (ab[1]*ac[2] - ab[2]*ac[1], ab[2]*ac[0] - ab[0]*ac[2], ab[0]*ac[1] - ab[1]*ac[0])
            area2 = math.sqrt(sum(v*v for v in cross))
            if area2 <= 1e-12:
                degenerate += 1
                continue
            # A cyclic rotation identifies the same oriented triangle while
            # retaining intentional reversed faces on double-sided planes.
            keys = [tuple(round(x, 8) for x in p) for p in points]
            canonical = min(tuple(keys[i:] + keys[:i]) for i in range(3))
            if canonical in oriented_triangles:
                duplicate += 1
                continue
            oriented_triangles.add(canonical)
            # Geometry is currently flat-shaded block meshes. Recomputing the
            # normalized face normal from the final transformed triangle is
            # equivalent to inverse-transpose normal conversion and stays
            # aligned after negative-scale winding correction.
            normal = tuple(v / area2 for v in cross)
            start = len(vertices)
            for point in points:
                vertices.append((*point, *normal))
            indices.extend((start, start + 1, start + 2))
        if degenerate:
            raise ValueError(f"{obj.name}: encountered {degenerate} degenerate triangles")
        return vertices, indices, {"duplicate_triangles_removed": duplicate}
    finally:
        evaluated_obj.to_mesh_clear()


def _mesh_components(mesh):
    adjacency = [set() for _ in mesh.vertices]
    for edge in mesh.edges:
        a, b = edge.vertices
        adjacency[a].add(b)
        adjacency[b].add(a)
    seen = set()
    components = []
    for start in range(len(adjacency)):
        if start in seen:
            continue
        stack = [start]
        seen.add(start)
        component = []
        while stack:
            current = stack.pop()
            component.append(current)
            for neighbor in adjacency[current]:
                if neighbor not in seen:
                    seen.add(neighbor)
                    stack.append(neighbor)
        components.append(component)
    return components


def _collision_boxes(obj, anchor_world):
    boxes = []
    for component in _mesh_components(obj.data):
        points = []
        for index in component:
            v = obj.data.vertices[index]
            points.append(_canonical_position(obj, v, anchor_world))
        minimum = [min(point[i] for point in points) for i in range(3)]
        maximum = [max(point[i] for point in points) for i in range(3)]
        boxes.append([*minimum, *maximum])
    return boxes


def _state_record(obj):
    return {
        "state_id": int(obj.get("state_id", obj.get("fc_state_id"))),
        "state_properties": _json_property(obj, "state_properties", obj.get("fc_state_properties", "{}")) or {},
        "geometry_type": str(obj.get("geometry_type", obj.get("fc_visual_classification", "unknown"))),
        "collision_type": str(obj.get("collision_type", obj.get("fc_collision_classification", "unknown"))),
        "source_models": _json_property(obj, "fc_source_models", "[]") or [],
        "source_shape_id": int(obj.get("source_shape_id", obj.get("fc_collision_shape_id", 0)) or 0),
        "collision_boxes": [],
    }


def _material_for(obj):
    if not obj.data.materials or obj.data.materials[0] is None:
        return None
    material = obj.data.materials[0]
    return {
        "name": material.name,
        "base_color_rgba": [float(v) for v in material.diffuse_color],
    }


def _prototype_anchor(scene, block_id, prototype_id, visual_objects):
    anchor_obj = _choose_anchor(visual_objects)
    anchor_world = _anchor_world(anchor_obj)
    anchor_cell = json.loads(anchor_obj.get("fc_mc_cell", "[0,0,0]"))
    return anchor_obj, anchor_world, [int(v) for v in anchor_cell]


def _validate_source_payload(scene, block_id, states):
    text = bpy.data.texts.get("FC_Catalogue_Payload")
    if not text:
        return {"status": "unverified", "reason": "embedded FC_Catalogue_Payload text is missing"}
    payload = json.loads(text.as_string())
    source_instances = [i for i in payload.get("instances", []) if i.get("block") == block_id]
    by_state = {int(i["state_id"]): i for i in source_instances}
    results = []
    for state in states:
        item = by_state.get(state["state_id"])
        if not item:
            results.append({"state_id": state["state_id"], "status": "unverified", "reason": "state absent from embedded payload"})
            continue
        expected = item.get("collision", {})
        expected_boxes = expected.get("boxes", [])
        actual = state.get("collision_boxes", [])
        cell = item.get("cell", [0, 0, 0])
        anchor_cell = state.get("_anchor_cell", [0, 0, 0])
        delta = [cell[0]-anchor_cell[0], cell[1]-anchor_cell[1], cell[2]-anchor_cell[2]]
        transformed = []
        for box in expected_boxes:
            x0,y0,z0,x1,y1,z1 = [float(v) for v in box]
            transformed.append([x0-.5+delta[0], y0+delta[1], z0-.5+delta[2], x1-.5+delta[0], y1+delta[1], z1-.5+delta[2]])
        quant = lambda boxes: sorted(tuple(round(float(v), 6) for v in box) for box in boxes)
        matches = quant(transformed) == quant(actual)
        results.append({"state_id": state["state_id"], "status": "pass" if matches else "fail", "shape_id": expected.get("shape_id"), "box_count": len(expected_boxes)})
    return {"status": "pass" if all(r["status"] == "pass" for r in results) else "fail", "states": results}


def _validate_exported_geometry(components, states):
    failures = []
    for component in components:
        vertices = component["_vertices"]
        indices = component["_indices"]
        signed_volume = 0.0
        if len(indices) % 3:
            failures.append(f"{component['name']}: index count is not triangular")
        if any(i < 0 or i >= len(vertices) for i in indices):
            failures.append(f"{component['name']}: index out of range")
        for tri in range(0, len(indices), 3):
            points = [vertices[indices[tri+j]][:3] for j in range(3)]
            a,b,c=points
            ab=(b[0]-a[0],b[1]-a[1],b[2]-a[2]);ac=(c[0]-a[0],c[1]-a[1],c[2]-a[2])
            cross=(ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
            if sum(v*v for v in cross) <= 1e-20:
                failures.append(f"{component['name']}: degenerate triangle")
                break
            normals=[vertices[indices[tri+j]][3:] for j in range(3)]
            if any(not math.isfinite(v) for p in points for v in p) or any(not math.isfinite(v) for n in normals for v in n):
                failures.append(f"{component['name']}: non-finite float")
                break
            if any(abs(math.sqrt(sum(v*v for v in n))-1.0)>1e-5 for n in normals):
                failures.append(f"{component['name']}: non-unit normal")
                break
            if any(sum(cross[k]*n[k] for k in range(3)) <= 0 for n in normals):
                failures.append(f"{component['name']}: winding/normal mismatch")
                break
            signed_volume += (a[0]*(b[1]*c[2]-b[2]*c[1]) + a[1]*(b[2]*c[0]-b[0]*c[2]) + a[2]*(b[0]*c[1]-b[1]*c[0])) / 6.0
        # Closed prototype meshes have positive signed volume when their
        # triangles face outward. Double-sided/open planes have zero volume.
        if signed_volume < -1e-10:
            failures.append(f"{component['name']}: inward-facing closed surface")
    return failures


def _safe_stem(block_id):
    return block_id.replace(":", "_").replace("/", "_")


def _serialize_component(component, binary):
    vertex_offset = len(binary)
    for vertex in component["_vertices"]:
        binary.extend(VERTEX.pack(*vertex))
    index_offset = len(binary)
    for value in component["_indices"]:
        binary.extend(INDEX.pack(int(value)))
    public = {k: v for k, v in component.items() if not k.startswith("_")}
    public.update({
        "vertex_count": len(component["_vertices"]),
        "index_count": len(component["_indices"]),
        "triangle_count": len(component["_indices"]) // 3,
        "vertex_buffer": {"byte_offset": vertex_offset, "byte_length": len(component["_vertices"]) * VERTEX.size},
        "index_buffer": {"byte_offset": index_offset, "byte_length": len(component["_indices"]) * INDEX.size},
    })
    return public


def export_scene(scene_name=DEFAULT_SCENE, output_dir=None, selected_block_ids=None):
    if bpy is None:
        raise RuntimeError("export_scene must run inside Blender")
    if output_dir is None:
        raise ValueError("output_dir is required")
    scene = bpy.data.scenes.get(scene_name)
    if scene is None:
        raise ValueError(f"Blender scene {scene_name!r} does not exist")
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    groups = _prototype_groups(scene, selected_block_ids)
    if selected_block_ids:
        missing = sorted(set(selected_block_ids) - {k[0] for k in groups})
        if missing:
            raise ValueError("requested blocks are missing visual metadata: " + ", ".join(missing))
    if not groups:
        raise ValueError("no FineCraft visual prototypes found in scene")

    export_report = {
        "schema_version": SCHEMA_VERSION,
        "scene": scene_name,
        "export_mode": "selected_blocks" if selected_block_ids else "all_prototypes",
        "requested_block_ids": sorted(set(selected_block_ids)) if selected_block_ids else None,
        "collision_source_validation": "mesh-island bounds compared with embedded FC_Catalogue_Payload boxes",
        "coordinate_tolerance": COORD_TOLERANCE,
        "coordinate_round_trip_samples": [],
        "assets": [],
        "source_collision_validation": {},
        "status": "pass",
    }
    for sample in [(0,0,0), (1,1,1), (-3,12,7), (.125,.375,.9375)]:
        restored = round_trip(sample)
        passed = all(abs(restored[i]-sample[i]) <= COORD_TOLERANCE for i in range(3))
        export_report["coordinate_round_trip_samples"].append({"input_blender": sample, "round_trip_blender": restored, "pass": passed})
        if not passed:
            export_report["status"] = "fail"

    asset_paths = []
    for (block_id, prototype_id), group in sorted(groups.items()):
        visual_objects = sorted(group["visual"], key=lambda o: (int(o.get("state_id", o.get("fc_state_id", 0))), str(o.get("fc_part", "")), o.name))
        if not visual_objects:
            continue
        anchor_obj, anchor_world, anchor_cell = _prototype_anchor(scene, block_id, prototype_id, visual_objects)
        first = visual_objects[0]
        source_commit = str(first.get("source_commit", first.get("fc_catalogue_commit", "unknown")))
        states_by_id = {}
        components = []
        for obj in visual_objects:
            record = states_by_id.setdefault(int(obj.get("state_id", obj.get("fc_state_id"))), _state_record(obj))
            source_models = record["source_models"]
            vertices, indices, tri_report = _triangulate_component(obj, anchor_world)
            components.append({
                "name": str(obj.get("fc_part", obj.name)),
                "state_id": record["state_id"],
                "source_models": source_models,
                "material": _material_for(obj),
                "_vertices": vertices,
                "_indices": indices,
                "_triangulation": tri_report,
            })
        states = [states_by_id[key] for key in sorted(states_by_id)]
        # Collider identity is matched by state_id; connected mesh islands stay
        # individual AABBs, never a single combined bounds box.
        collision_by_state = defaultdict(list)
        for obj in group["collision"]:
            state_id = int(obj.get("state_id", obj.get("fc_state_id", -1)))
            collision_by_state[state_id].extend(_collision_boxes(obj, anchor_world))
        for state in states:
            state["collision_boxes"] = sorted(collision_by_state.get(state["state_id"], []))
            state["_anchor_cell"] = anchor_cell
        validation_errors = _validate_exported_geometry(components, states)
        if validation_errors:
            raise ValueError("geometry validation failed for " + block_id + ": " + "; ".join(validation_errors))
        source_validation = _validate_source_payload(scene, block_id, states)
        if source_validation["status"] == "fail":
            raise ValueError(f"collision proxies do not match embedded source payload for {block_id}: {source_validation}")
        export_report["source_collision_validation"][block_id] = source_validation

        binary_path = output_dir / (_safe_stem(block_id) + ".geom")
        json_path = output_dir / (_safe_stem(block_id) + ".json")
        binary = bytearray(BINARY_HEADER.pack(BINARY_MAGIC, BINARY_VERSION, BINARY_HEADER.size, len(components)))
        public_components = [_serialize_component(c, binary) for c in components]
        binary_path.write_bytes(binary)
        metadata = {
            "schema_version": SCHEMA_VERSION,
            "format": "finecraft-hybrid-geometry",
            "minecraft_version": "26.2",
            "source_commit": source_commit,
            "coordinate_system": {
                "space": "prototype_local_minecraft",
                "axis_order": ["x_east", "y_up", "z_south"],
                "units_per_block": 1.0,
                "prototype_origin": "bottom_centre_of_lower_or_foot_cell",
                "blender_to_minecraft": "(x, z, -y)",
                "layout_offsets_removed": True,
                "coordinate_tolerance": COORD_TOLERANCE,
            },
            "block_id": block_id,
            "prototype_id": int(prototype_id),
            "geometry_type": str(first.get("geometry_type", first.get("fc_visual_classification", "unknown"))),
            "collision_type": str(first.get("collision_type", first.get("fc_collision_classification", "unknown"))),
            "source_models": sorted({model for state in states for model in state["source_models"]}),
            "states": [{k:v for k,v in state.items() if not k.startswith("_")} for state in states],
            "geometry": {
                "binary_file": binary_path.name,
                "header": {"magic_ascii": BINARY_MAGIC.decode("ascii"), "version": BINARY_VERSION, "header_bytes": BINARY_HEADER.size, "component_count": len(components)},
                "vertex_layout": {"encoding": "float32_le", "stride_bytes": VERTEX.size, "attributes": [{"name":"position","components":3,"offset_bytes":0},{"name":"normal","components":3,"offset_bytes":12}]},
                "index_layout": {"encoding": "uint32_le", "element_bytes": INDEX.size},
                "components": public_components,
            },
        }
        json_path.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        asset_entry = {
            "block_id": block_id,
            "prototype_id": int(prototype_id),
            "metadata_file": json_path.name,
            "geometry_file": binary_path.name,
            "metadata_bytes": json_path.stat().st_size,
            "geometry_bytes": binary_path.stat().st_size,
            "geometry_sha256": hashlib.sha256(binary_path.read_bytes()).hexdigest(),
            "visual_component_count": len(components),
            "triangle_count": sum(c["triangle_count"] for c in public_components),
            "collision_box_count": sum(len(s["collision_boxes"]) for s in metadata["states"]),
            "state_ids": [s["state_id"] for s in metadata["states"]],
            "duplicate_triangles_removed": sum(c["_triangulation"]["duplicate_triangles_removed"] for c in components),
        }
        export_report["assets"].append(asset_entry)
        asset_paths.append(str(json_path))

    if export_report["source_collision_validation"] and any(v["status"] != "pass" for v in export_report["source_collision_validation"].values()):
        export_report["status"] = "unverified"
    export_report["summary"] = {
        "prototype_count": len(export_report["assets"]),
        "state_record_count": sum(len(asset["state_ids"]) for asset in export_report["assets"]),
        "visual_component_count": sum(asset["visual_component_count"] for asset in export_report["assets"]),
        "triangle_count": sum(asset["triangle_count"] for asset in export_report["assets"]),
        "collision_box_count": sum(asset["collision_box_count"] for asset in export_report["assets"]),
        "metadata_bytes": sum(asset["metadata_bytes"] for asset in export_report["assets"]),
        "geometry_bytes": sum(asset["geometry_bytes"] for asset in export_report["assets"]),
    }
    report_path = output_dir.parent / "reports" / "export_report.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(export_report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    export_report["report_file"] = "../reports/export_report.json"
    export_report["metadata_files"] = [Path(path).name for path in asset_paths]
    return export_report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", default=DEFAULT_SCENE)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--blocks", help="comma-separated Minecraft block IDs")
    parser.add_argument("--all-prototypes", action="store_true", help="export every prototype found in the scene")
    args = parser.parse_args(argv)
    selected = None if args.all_prototypes or not args.blocks else [b.strip() for b in args.blocks.split(",") if b.strip()]
    report = export_scene(args.scene, args.output_dir, selected)
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else None)
