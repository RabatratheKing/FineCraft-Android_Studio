import importlib.util
import json
import math
import struct
import unittest
from collections import Counter
from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
EXPORT_DIR = REPO / "app/src/main/assets/geometry/prototypes"
REPORT_FILE = REPO / "app/src/main/assets/geometry/reports/export_report.json"
BLOCKS_FILE = REPO / "app/src/main/assets/catalogue/blocks_master.json"
COLLISION_FILE = REPO / "app/src/main/assets/catalogue/collision_shapes.json"
EXPECTED_COMMIT = "cc78e8696b61b0a20099cccfa42348b5052ff28d"
EXPECTED_STATES = {
    "minecraft:stone": {1: {}},
    "minecraft:oak_stairs": {3918: {"facing": "north", "half": "bottom", "shape": "straight", "waterlogged": "false"}},
    "minecraft:torch": {3370: {}},
    "minecraft:oak_door": {
        5666: {"facing": "north", "half": "lower", "hinge": "left", "open": "false", "powered": "false"},
        5658: {"facing": "north", "half": "upper", "hinge": "left", "open": "false", "powered": "false"},
    },
}
EXPECTED_COLLISIONS = {
    "minecraft:stone": {1: (1, 1)},
    "minecraft:oak_stairs": {3918: (33, 2)},
    "minecraft:torch": {3370: (0, 0)},
    "minecraft:oak_door": {5666: (59, 1), 5658: (59, 1)},
}
TOLERANCE = 1e-6


def read_asset(metadata_path):
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    binary = (metadata_path.parent / metadata["geometry"]["binary_file"]).read_bytes()
    magic, version, header_bytes, component_count = struct.unpack_from("<4sHHI", binary)
    return metadata, binary, (magic, version, header_bytes, component_count)


def triangles_for(metadata, binary, component):
    geometry = metadata["geometry"]
    vb = component["vertex_buffer"]
    ib = component["index_buffer"]
    vertices = [struct.unpack_from("<6f", binary, vb["byte_offset"] + i * 24) for i in range(component["vertex_count"])]
    indices = [struct.unpack_from("<I", binary, ib["byte_offset"] + i * 4)[0] for i in range(component["index_count"])]
    return vertices, indices


class GeometryExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assets = {}
        for path in sorted(EXPORT_DIR.glob("*.json")):
            metadata, binary, header = read_asset(path)
            cls.assets[metadata["block_id"]] = (path, metadata, binary, header)
        cls.blocks = json.loads(BLOCKS_FILE.read_text(encoding="utf-8"))
        cls.collision = json.loads(COLLISION_FILE.read_text(encoding="utf-8"))
        cls.report = json.loads(REPORT_FILE.read_text(encoding="utf-8"))

    def test_only_selected_prototypes_are_exported(self):
        self.assertEqual(set(self.assets), set(EXPECTED_STATES))
        self.assertEqual(len(self.assets), 4)
        self.assertEqual(len({m[1]["block_id"] for m in self.assets.values()}), 4)

    def test_metadata_states_provenance_and_schema(self):
        for block_id, expected_states in EXPECTED_STATES.items():
            _, metadata, _, header = self.assets[block_id]
            self.assertEqual(metadata["schema_version"], 1)
            self.assertEqual(metadata["format"], "finecraft-hybrid-geometry")
            self.assertEqual(metadata["minecraft_version"], "26.2")
            self.assertEqual(metadata["source_commit"], EXPECTED_COMMIT)
            self.assertEqual(metadata["coordinate_system"]["space"], "prototype_local_minecraft")
            self.assertTrue(metadata["coordinate_system"]["layout_offsets_removed"])
            self.assertEqual(header, (b"FCGB", 1, 12, len(metadata["geometry"]["components"])))
            got = {s["state_id"]: s for s in metadata["states"]}
            self.assertEqual(set(got), set(expected_states))
            for state_id, props in expected_states.items():
                self.assertEqual(got[state_id]["state_properties"], props)
                self.assertIn("geometry_type", got[state_id])
                self.assertIn("collision_type", got[state_id])
                self.assertIn("source_models", got[state_id])
                self.assertIn("source_shape_id", got[state_id])

    def test_collision_data_matches_authoritative_catalogue(self):
        catalogue_blocks = {b["registry"]["identifier"]: b for b in self.blocks["blocks"]}
        for block_id, expected_states in EXPECTED_COLLISIONS.items():
            _, metadata, _, _ = self.assets[block_id]
            states = {s["state_id"]: s for s in metadata["states"]}
            for state_id, (shape_id, count) in expected_states.items():
                state = states[state_id]
                self.assertEqual(state["source_shape_id"], shape_id)
                self.assertEqual(len(state["collision_boxes"]), count)
                mapped_shape = self.collision["block_state_mappings"][block_id][str(state_id)]
                self.assertEqual(mapped_shape, shape_id)
                raw_boxes = self.collision["shapes"][str(shape_id)]
                self.assertEqual(len(raw_boxes), count)
                # State cell offset preserves door upper-half placement relative
                # to its lower-cell prototype anchor.
                door_dy = 1 if block_id == "minecraft:oak_door" and state_id == 5658 else 0
                expected = sorted(tuple(round(x, 6) for x in (
                    box[0]-.5, box[1]+door_dy, box[2]-.5,
                    box[3]-.5, box[4]+door_dy, box[5]-.5,
                )) for box in raw_boxes)
                actual = sorted(tuple(round(float(x), 6) for x in box) for box in state["collision_boxes"])
                self.assertEqual(actual, expected)
        self.assertEqual(sum(len(s["collision_boxes"]) for _, m, _, _ in self.assets.values() for s in m["states"]), 5)

    def test_binary_triangles_normals_and_bounds(self):
        for block_id, (_, metadata, binary, header) in self.assets.items():
            self.assertEqual(header[0], b"FCGB")
            self.assertEqual(header[3], len(metadata["geometry"]["components"]))
            self.assertEqual(metadata["geometry"]["vertex_layout"]["stride_bytes"], 24)
            component_names = set()
            for component in metadata["geometry"]["components"]:
                self.assertNotIn((component["name"], component["state_id"]), component_names)
                component_names.add((component["name"], component["state_id"]))
                vertices, indices = triangles_for(metadata, binary, component)
                signed_volume = 0.0
                self.assertEqual(component["index_count"] % 3, 0)
                self.assertEqual(component["triangle_count"], len(indices)//3)
                self.assertTrue(all(0 <= i < len(vertices) for i in indices))
                for p in vertices:
                    self.assertTrue(all(math.isfinite(v) for v in p))
                    self.assertAlmostEqual(math.sqrt(sum(v*v for v in p[3:])), 1.0, delta=1e-5)
                for offset in range(0, len(indices), 3):
                    ia, ib, ic = indices[offset:offset+3]
                    a, b, c = [vertices[i][:3] for i in (ia, ib, ic)]
                    ab = tuple(b[i]-a[i] for i in range(3))
                    ac = tuple(c[i]-a[i] for i in range(3))
                    cross = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
                    self.assertGreater(sum(x*x for x in cross), 1e-16)
                    signed_volume += (a[0]*(b[1]*c[2]-b[2]*c[1]) + a[1]*(b[2]*c[0]-b[0]*c[2]) + a[2]*(b[0]*c[1]-b[1]*c[0])) / 6.0
                    for index in (ia, ib, ic):
                        n = vertices[index][3:]
                        self.assertGreater(sum(cross[i]*n[i] for i in range(3)), 0)
                self.assertGreater(signed_volume, 0.0, f"{block_id}:{component['name']} must wind outward")
                # All POC blocks fit around the origin; the door's upper state
                # reaches at most two blocks high by design.
                for axis in range(3):
                    coords = [v[axis] for v in vertices]
                    self.assertGreaterEqual(min(coords), -0.500001)
                    upper = 2.000001 if block_id == "minecraft:oak_door" and axis == 1 else 1.000001
                    self.assertLessEqual(max(coords), upper)
                self.assertIsNone(component["material"]) if component["material"] is None else self.assertEqual(len(component["material"]["base_color_rgba"]), 4)

    def test_expected_component_and_triangle_counts(self):
        expected = {"minecraft:stone": (1, 12), "minecraft:oak_stairs": (1, 24), "minecraft:torch": (1, 12), "minecraft:oak_door": (2, 24)}
        for block_id, (components, triangles) in expected.items():
            metadata = self.assets[block_id][1]
            actual_components = metadata["geometry"]["components"]
            self.assertEqual(len(actual_components), components)
            self.assertEqual(sum(c["triangle_count"] for c in actual_components), triangles)

    def test_export_report_and_coordinate_round_trips(self):
        self.assertEqual(self.report["status"], "pass")
        self.assertEqual({a["block_id"] for a in self.report["assets"]}, set(EXPECTED_STATES))
        self.assertTrue(all(sample["pass"] for sample in self.report["coordinate_round_trip_samples"]))
        self.assertTrue(all(v["status"] == "pass" for v in self.report["source_collision_validation"].values()))
        for asset in self.report["assets"]:
            metadata_path = EXPORT_DIR / asset["metadata_file"]
            binary_path = EXPORT_DIR / asset["geometry_file"]
            meta_json = json.loads(metadata_path.read_text(encoding="utf-8"))
            self.assertEqual(meta_json["block_id"], asset["block_id"])
            self.assertEqual(binary_path.stat().st_size, asset["geometry_bytes"])


if __name__ == "__main__":
    unittest.main()
