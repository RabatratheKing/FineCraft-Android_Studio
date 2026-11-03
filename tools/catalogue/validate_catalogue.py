#!/usr/bin/env python3
"""Validate catalogue coverage, source integrity, shape data and determinism."""
from __future__ import annotations
import argparse
import json
import sys
import tempfile
from pathlib import Path
from generate_catalogue import (BEHAVIOUR_TAGS, CLASSIFICATIONS, DEFAULT_OUTPUT, DEFAULT_SOURCE,
                                SOURCE_COMMIT, build, stable_json)


def load(path): return json.loads(path.read_text(encoding="utf-8"))
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE)
    ap.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = ap.parse_args()
    errors, warnings = [], []
    source_blocks = load(args.source_dir / "blocks.json")
    source_collisions = load(args.source_dir / "blockCollisionShapes.json")
    source_names = {b["name"] for b in source_blocks}
    outputs = build(args.source_dir)
    master = outputs["blocks_master.json"]
    defs = master["blocks"]
    entries = {b["registry"]["internal_name"]: b for b in defs}
    if len(defs) != len(source_blocks): errors.append(f"catalogue count {len(defs)} != source count {len(source_blocks)}")
    if set(entries) != source_names:
        errors.append(f"source registry mismatch: missing={sorted(source_names-set(entries))}; extra={sorted(set(entries)-source_names)}")
    ids = [b["registry"]["id"] for b in defs]
    namespaced = [b["registry"]["identifier"] for b in defs]
    if len(ids) != len(set(ids)): errors.append("registry IDs are not unique")
    if len(namespaced) != len(set(namespaced)): errors.append("namespaced identifiers are not unique")
    src_by_name = {b["name"]: b for b in source_blocks}
    for entry in defs:
        r = entry["registry"]
        source = src_by_name.get(r["internal_name"])
        if source and (r["id"] != source["id"] or r["identifier"] != "minecraft:" + source["name"]):
            errors.append(f"source identity mismatch: {r['identifier']}")
        lo, hi, default = r["min_state_id"], r["max_state_id"], r["default_state_id"]
        if not (isinstance(lo, int) and isinstance(hi, int) and lo <= hi and lo <= default <= hi):
            errors.append(f"invalid state range/default: {r['identifier']}")
        state_count = hi-lo+1 if isinstance(lo, int) and isinstance(hi, int) and hi >= lo else -1
        if state_count != r["state_count"]: errors.append(f"state count mismatch: {r['identifier']}")
        product = 1
        for prop in r.get("states", []):
            n = prop.get("num_values")
            if not isinstance(n, int) or n < 1: errors.append(f"invalid state property count: {r['identifier']}:{prop.get('name')}")
            else: product *= n
            if isinstance(n, int) and len(prop.get("values", [])) != n: errors.append(f"state values mismatch: {r['identifier']}:{prop.get('name')}")
        if r.get("states") and product != state_count: warnings.append(f"state property product {product} differs from ID range {state_count}: {r['identifier']}")
        if not entry["classification"]["physical"] or not set(entry["classification"]["physical"]) <= CLASSIFICATIONS:
            errors.append(f"invalid physical classification: {r['identifier']}")
        if entry["properties"].get("light_emission") is not None and not (0 <= entry["properties"]["light_emission"] <= 15):
            errors.append(f"light emission outside 0..15: {r['identifier']}")
        tags = next((b["tags"] for b in outputs["block_behaviours.json"]["blocks"] if b["identifier"] == r["identifier"]), [])
        invalid = set(tags) - BEHAVIOUR_TAGS
        if invalid: errors.append(f"invalid behaviour tags for {r['identifier']}: {sorted(invalid)}")
    if len(ids) != len(source_blocks) or set(ids) != {b["id"] for b in source_blocks}: errors.append("source registry IDs are missing or remapped")
    if any("finecraft_id" in b or "legacy_block_id" in b for b in defs): errors.append("catalogue contains a reused/stale FineCraft ID field")
    mappings = outputs["collision_shapes.json"]["block_state_mappings"]
    shapes = outputs["collision_shapes.json"]["shapes"]
    for source_block in source_blocks:
        name = source_block["name"]
        mapping = mappings.get("minecraft:" + name)
        raw = source_collisions["blocks"].get(name)
        if mapping is None or raw is None: errors.append(f"missing collision mapping: minecraft:{name}"); continue
        ids_for_states = raw if isinstance(raw, list) else [raw] * (source_block["maxStateId"]-source_block["minStateId"]+1)
        if len(ids_for_states) != source_block["maxStateId"]-source_block["minStateId"]+1:
            errors.append(f"collision mapping length mismatch: minecraft:{name}")
        if len(mapping) != len(ids_for_states): errors.append(f"generated collision mapping length mismatch: minecraft:{name}")
        for shape_id in ids_for_states:
            boxes = shapes.get(str(shape_id))
            if boxes is None: errors.append(f"unresolved shape reference {shape_id}: minecraft:{name}"); continue
            for box in boxes:
                if len(box) != 6 or any(not isinstance(v, (int, float)) for v in box):
                    errors.append(f"invalid bounds tuple: shape {shape_id}"); continue
                if any(box[i] > box[i+3] for i in range(3)): errors.append(f"inverted bounds: shape {shape_id}: {box}")
    expected_additions = {"sulfur", "potent_sulfur", "sulfur_slab", "sulfur_stairs", "sulfur_wall", "polished_sulfur", "polished_sulfur_slab", "polished_sulfur_stairs", "polished_sulfur_wall", "sulfur_bricks", "sulfur_brick_slab", "sulfur_brick_stairs", "sulfur_brick_wall", "chiseled_sulfur", "cinnabar", "cinnabar_slab", "cinnabar_stairs", "cinnabar_wall", "polished_cinnabar", "polished_cinnabar_slab", "polished_cinnabar_stairs", "polished_cinnabar_wall", "cinnabar_bricks", "cinnabar_brick_slab", "cinnabar_brick_stairs", "cinnabar_brick_wall", "chiseled_cinnabar", "sulfur_spike"}
    missing_additions = expected_additions - source_names
    if missing_additions: errors.append(f"26.2 source additions missing: {sorted(missing_additions)}")
    for required in ["potent_sulfur", "sulfur_spike"]:
        found = entries.get(required)
        if not found: errors.append(f"missing special 26.2 block: {required}")
    if any(b["registry"]["identifier"].endswith("_bed") and b["classification"]["primary_geometry"] != "multipart_structure" for b in defs):
        errors.append("bed multipart classification missing")
    # Build twice and compare byte-for-byte with both the second build and generated files.
    first = {name: stable_json(data) for name, data in outputs.items()}
    second = {name: stable_json(data) for name, data in build(args.source_dir).items()}
    if first != second: errors.append("repeated generation is not deterministic")
    for name, expected in first.items():
        path = args.output_dir / name
        if not path.exists(): errors.append(f"generated output missing: {name}")
        elif path.read_bytes() != expected: errors.append(f"generated output differs from pinned inputs: {name}")
    warnings += outputs["catalogue_audit.json"]["warnings"]
    missing_item_count = outputs["catalogue_statistics.json"]["blocks_without_matching_item"]
    if missing_item_count:
        warnings.append(f"{missing_item_count} blocks have no exact-name item entry in the source item registry; identifiers are listed in the audit.")
    report = {"status": "passed_with_unresolved_fields" if not errors else "failed", "source_commit": SOURCE_COMMIT,
              "source_registry_count": len(source_blocks), "catalogued_count": len(defs), "errors": errors,
              "warnings": sorted(set(warnings)), "unresolved": outputs["catalogue_audit.json"]["unresolved"],
              "checks": {"registry_completeness": not errors, "unique_ids_and_identifiers": len(ids)==len(set(ids)) and len(namespaced)==len(set(namespaced)),
                         "collision_state_coverage": sum(v is not None for v in mappings.values()),
                         "collision_shape_count": len(shapes), "deterministic_repeated_build": first==second,
                         "26_2_additions_present": not missing_additions}}
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "catalogue_validation_report.json").write_bytes(stable_json(report))
    print(json.dumps({k: report[k] for k in ("status", "source_registry_count", "catalogued_count", "errors", "warnings")}, indent=2))
    return 1 if errors else 0
if __name__ == "__main__": sys.exit(main())
