#!/usr/bin/env python3
"""Audit catalogue, Minecraft 26.2 server/client evidence, collisions and state coverage."""
from __future__ import annotations
import argparse, json, math, sys
from collections import Counter
from pathlib import Path
from generate_catalogue import (BEHAVIOUR_TAGS, DEFAULT_OUTPUT, DEFAULT_SOURCE, GEOMETRY_CLASSES,
                                SOURCE_COMMIT, build, stable_json)

def load(path): return json.loads(path.read_text(encoding="utf-8"))
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--source-dir",type=Path,default=DEFAULT_SOURCE); ap.add_argument("--output-dir",type=Path,default=DEFAULT_OUTPUT); a=ap.parse_args()
    repo=Path(__file__).resolve().parents[2]; official_dir=Path(__file__).resolve().parent/"source"/"official_26.2"
    errors=[]; warnings=[]; checks={}
    src=load(a.source_dir/"blocks.json"); coll=load(a.source_dir/"blockCollisionShapes.json"); official=load(official_dir/"blocks.json"); regs=load(official_dir/"registries.json"); models=load(official_dir/"client_geometry.json")
    out=build(a.source_dir); master=out["blocks_master.json"]; defs=master["blocks"]; by_name={e["registry"]["internal_name"]:e for e in defs}
    src_by={b["name"]:b for b in src}; official_by={k.split(":",1)[1]:v for k,v in official.items()}
    if len(defs)!=len(src) or set(by_name)!=set(src_by): errors.append("block registry count/identifier coverage differs from source")
    ids=[e["registry"]["id"] for e in defs]; names=[e["registry"]["identifier"] for e in defs]
    if len(set(ids))!=len(ids) or len(set(names))!=len(names): errors.append("registry IDs or namespaced identifiers are duplicated")
    if len(official)!=len(defs): errors.append(f"official server report block count {len(official)} != catalogue {len(defs)}")
    state_count=0; property_count=0; state_mismatches=[]; id_set=set()
    for name,e in by_name.items():
        r=e["registry"]; s=src_by[name]; o=official_by.get(name,{}); sidset=set(range(r["min_state_id"],r["max_state_id"]+1)); state_count+=r["state_count"]
        if (r["id"],r["min_state_id"],r["max_state_id"],r["default_state_id"])!=(s["id"],s["minStateId"],s["maxStateId"],s["defaultState"]): errors.append(f"upstream identity/state range differs for {name}")
        if id_set & sidset: errors.append(f"overlapping state ID range: {name}")
        id_set |= sidset
        props=r.get("states",[]); property_count+=len(props)
        if len(o.get("states",[]))!=r["state_count"]: state_mismatches.append(name+":state_count"); continue
        if {x["id"] for x in o["states"]}!=sidset: state_mismatches.append(name+":state_ids")
        if {p["name"] for p in props}!={k for k in o.get("properties",{})}: state_mismatches.append(name+":property_names")
        # Compare every state tuple using normalized string values against explicit state domains.
        domains={p["name"]:{str(v).lower() for v in p.get("values",[])} for p in props}
        for state in o.get("states",[]):
            values=state.get("properties",{})
            if set(values)!=set(domains) or any(str(values[k]).lower() not in domains[k] for k in values):
                state_mismatches.append(f"{name}:state_{state['id']}_properties"); break
        cls=e.get("classification",{}); primary=cls.get("primary_geometry")
        if primary not in GEOMETRY_CLASSES: errors.append(f"invalid primary geometry: {name}:{primary}")
        if len(cls)!=1: errors.append(f"geometry is not exactly one primary class: {name}")
        tags=next((b["tags"] for b in out["block_behaviours.json"]["blocks"] if b["identifier"]=="minecraft:"+name),[])
        if len(tags)!=len(set(tags)) or set(tags)- (BEHAVIOUR_TAGS|{"technical","fluid"}): errors.append(f"invalid or duplicate behavior tags: {name}")
        if e["properties"].get("light_emission") is not None and not 0<=e["properties"]["light_emission"]<=15: errors.append(f"invalid light emission: {name}")
    if state_mismatches: errors.append(f"official state/property mismatches: {len(state_mismatches)}; examples={state_mismatches[:12]}")
    checks["official_state_IDs_and_properties"]={"states_compared":state_count,"properties_compared":property_count,"mismatch_count":len(state_mismatches),"mismatch_examples":state_mismatches[:20]}
    # Validate every source collision row, every state link, every box and duplicate geometry shape.
    shapes=out["collision_shapes.json"]["shapes"]; mappings=out["collision_shapes.json"]["block_state_mappings"]; used=set(); collision_rows=0; box_count=0
    for b in src:
        name=b["name"]; raw=coll["blocks"].get(name); mapping=mappings.get("minecraft:"+name); n=b["maxStateId"]-b["minStateId"]+1
        raw_ids=raw if isinstance(raw,list) else ([raw]*n if isinstance(raw,int) else None)
        if raw_ids is None or len(raw_ids)!=n or not isinstance(mapping,dict) or set(mapping)!={str(x) for x in range(b["minStateId"],b["maxStateId"]+1)}:
            errors.append(f"incomplete collision state mapping: {name}"); continue
        if len(raw_ids)!=n: continue
        for ix,sid in enumerate(raw_ids):
            collision_rows+=1; state_id=b["minStateId"]+ix; sidtext=str(sid); boxes=shapes.get(sidtext)
            if mapping.get(str(state_id))!=sid: errors.append(f"collision state remap mismatch: {name}:{state_id}")
            if boxes is None: errors.append(f"missing referenced shape: {name}:{state_id}->{sid}"); continue
            used.add(sidtext)
            if not isinstance(boxes,list): errors.append(f"shape must be a list of boxes: {sid}"); continue
            for box in boxes:
                box_count+=1
                if not isinstance(box,list) or len(box)!=6 or any(not isinstance(v,(int,float)) or not math.isfinite(v) for v in box): errors.append(f"invalid/non-finite bounds: shape {sid}"); continue
                if any(box[i]>box[i+3] for i in range(3)): errors.append(f"inverted box: shape {sid}: {box}")
    sigs=Counter(json.dumps(v,separators=(",",":"),sort_keys=True) for v in shapes.values()); unique=len(sigs)
    if set(shapes)-used: warnings.append(f"{len(set(shapes)-used)} shape IDs are not referenced by any block-state map")
    checks["collision"]={"block_mappings":len(mappings),"expected_block_mappings":len(src),"state_mappings_checked":collision_rows,"expected_states":state_count,"state_mapping_gaps":state_count-collision_rows,"shape_IDs":len(shapes),"referenced_shape_IDs":len(used),"unreferenced_shape_IDs":len(set(shapes)-used),"box_count":box_count,"unique_exact_shape_geometries":unique,"duplicate_geometry_shape_IDs":len(shapes)-unique,"geometry_errors":len(errors)}
    # Official client assets: all catalogue blockstates and referenced models must resolve.
    model_missing=[]; refs_missing=[]
    for name in by_name:
        state=models["blockstates"].get(name)
        if state is None:model_missing.append(name); continue
        refs=set()
        def walk(v):
            if isinstance(v,dict):
                if isinstance(v.get("model"),str):refs.add(v["model"])
                for x in v.values():walk(x)
            elif isinstance(v,list):
                for x in v:walk(x)
        walk(state)
        refs_missing += [f"{name}->{r}" for r in refs if r not in models["models"]]
    if model_missing or refs_missing: errors.append(f"client model refs missing: blocks={len(model_missing)}, refs={len(refs_missing)}")
    checks["client_models"]={"blockstates_found":len(by_name)-len(model_missing),"missing_blockstates":len(model_missing),"model_references_missing":len(refs_missing),"client_jar_sha1":models["source"]["client_jar_sha1"]}
    # Block-entity registry mapping must cover all registered types; all nonmatches remain explicitly known absent.
    mapped={b["block_entity"]["type"] for b in out["block_behaviours.json"]["blocks"] if b["block_entity"]["type"]}
    registry_entities=set(regs["minecraft:block_entity_type"]["entries"])
    if mapped!=registry_entities: errors.append(f"block entity registry coverage differs: missing={sorted(registry_entities-mapped)}, extra={sorted(mapped-registry_entities)}")
    checks["block_entities"]={"registry_types":len(registry_entities),"types_mapped_to_blocks":len(mapped),"blocks_with_type":sum(bool(e["block_entity"]["type"]) for e in defs),"known_absent_blocks":sum(not e["block_entity"]["type"] for e in defs)}
    # Item relation counts are evidence labels only, no guessed aliases are promoted.
    item_report=out["block_item_relationships.json"]; component_items=set(item_report["official_item_block_state_components"]); missing_names={x["block"] for x in item_report["blocks"] if x["status"]=="no_exact_name_item"};
    if len(component_items)!=12 or any(name not in {"minecraft:"+n for n in by_name} for name in component_items) or component_items & missing_names: errors.append("official item block_state component evidence inconsistent")
    exact=sum(x["status"]=="exact_name_item" for x in item_report["blocks"])
    if len(item_report["blocks"])!=len(defs) or exact!=1042: errors.append("item relationship coverage/count inconsistent")
    checks["item_relationships"]={**item_report["counts"],"official_item_block_state_components":len(component_items),"components_overlap_missing_exact_name":len(component_items & missing_names),"total_blocks":len(item_report["blocks"])}
    # Example semantic regression guards for requested independent dimensions.
    for name,geom,tag in [("sand","full_cube","gravity_affected"),("glowstone","full_cube","light_emitter"),("torch","partial_geometry","support_dependent")]:
        e=by_name.get(name); tags=next(b["tags"] for b in out["block_behaviours.json"]["blocks"] if b["identifier"]=="minecraft:"+name)
        if not e or e["classification"]["primary_geometry"]!=geom or tag not in tags:errors.append(f"independent geometry/behavior example failed: {name}")
    checks["classification_consistency"]={"geometry_classes":out["catalogue_audit.json"]["classification_consistency"]["geometry_classes"],"exactly_one_per_block":True,"gravity_is_behavior_only":"gravity_affected" not in master["classification_values"]}
    # Reproducible generation and files on disk.
    first={k:stable_json(v) for k,v in out.items()}; second={k:stable_json(v) for k,v in build(a.source_dir).items()}
    if first!=second:errors.append("generation differs between repeated runs")
    a.output_dir.mkdir(parents=True,exist_ok=True)
    for name,data in first.items():
        f=a.output_dir/name
        if not f.exists() or f.read_bytes()!=data: errors.append(f"generated file differs from reproducible output: {name}")
    report={"schema_version":2,"status":"passed_with_unresolved_fields" if not errors else "failed","source_commit":SOURCE_COMMIT,"source_registry_count":len(src),"catalogued_count":len(defs),"state_count":state_count,"state_property_count":property_count,"errors":errors,"warnings":sorted(set(warnings) | set(out["catalogue_audit.json"]["warnings"])),"unresolved":out["catalogue_audit.json"]["unresolved"],"checks":checks,"limitations":["JSON and asset checks establish source consistency, not pixel-perfect rendering or live in-game collision/support behavior.","Selection, interaction, support update rules, neighbor update rules, render alpha/layer, and fire spread require runtime/game-code evidence.","Exact-name item matches are direct; candidate drop relationships are explicitly unverified."]}
    (a.output_dir/"catalogue_validation_report.json").write_bytes(stable_json(report))
    print(json.dumps({k:report[k] for k in ("status","source_registry_count","catalogued_count","state_count","state_property_count","errors","checks")},indent=2))
    return 1 if errors else 0
if __name__=="__main__":sys.exit(main())
