#!/usr/bin/env python3
"""Generate FineCraft's Minecraft Java 26.2 block catalogue from pinned JSON inputs."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = Path(__file__).resolve().parent / "source"
DEFAULT_OUTPUT = REPO / "app/src/main/assets/catalogue"
SOURCE_REPO = "https://github.com/Complexity-ML/minecraft-data-26.2"
SOURCE_COMMIT = "2a6a5fd9ebb0d964a73312d11beef596b7ec029b"
SOURCE_DATA_PATH = "data/pc/26.2"
CLASSIFICATIONS = {"full_cube", "gravity_affected", "partial_geometry", "multipart_structure", "non_solid", "fluid", "technical"}
GEOMETRY_CLASSES = {"full_cube", "partial_geometry", "multipart_structure", "non_solid", "fluid"}
BEHAVIOUR_TAGS = {
    "gravity_affected", "support_dependent", "full_collision", "partial_collision", "empty_collision",
    "compound_collision", "transparent", "translucent", "light_emitter", "waterloggable", "interactive",
    "block_entity", "multi_block", "animated", "redstone_component", "plant", "flammable", "slippery",
    "bouncy", "damaging", "special_rendering", "technical", "fluid"
}
GRAVITY_NAMES = {
    "sand", "red_sand", "gravel", "anvil", "chipped_anvil", "damaged_anvil", "dragon_egg",
    "pointed_dripstone", "suspicious_sand", "suspicious_gravel", "sulfur_spike"
}
TECHNICAL_NAMES = {
    "air", "cave_air", "void_air", "barrier", "light", "structure_void", "structure_block",
    "jigsaw", "command_block", "chain_command_block", "repeating_command_block", "structure_block",
    "end_gateway", "end_portal", "nether_portal", "moving_piston", "piston_head", "spawner",
    "trial_spawner", "vault", "test_block", "test_instance_block"
}
FLUID_NAMES = {"water", "lava"}
MULTIPART_NAMES = {
    "red_bed", "white_bed", "orange_bed", "magenta_bed", "light_blue_bed", "yellow_bed", "lime_bed",
    "pink_bed", "gray_bed", "light_gray_bed", "cyan_bed", "purple_bed", "blue_bed", "brown_bed",
    "green_bed", "black_bed", "sulfur_spike", "oak_door", "spruce_door", "birch_door", "jungle_door", "acacia_door",
    "cherry_door", "dark_oak_door", "mangrove_door", "bamboo_door", "crimson_door", "warped_door",
    "iron_door", "sunflower", "lilac", "rose_bush", "peony", "tall_grass", "large_fern"
}
INTERACTIVE_NAMES = {
    "chest", "trapped_chest", "barrel", "furnace", "blast_furnace", "smoker", "hopper", "dispenser",
    "dropper", "brewing_stand", "beacon", "lectern", "jukebox", "note_block", "chiseled_bookshelf",
    "bell", "beehive", "bee_nest", "decorated_pot", "crafter", "trial_spawner", "vault", "ender_chest",
    "shulker_box", "anvil", "chipped_anvil", "damaged_anvil", "enchanting_table", "smithing_table",
    "loom", "cartography_table", "grindstone", "stonecutter", "crafting_table", "respawn_anchor",
    "conduit", "comparator", "repeater", "lever", "daylight_detector", "target"
}
REDSTONE_NAMES = {
    "redstone_wire", "redstone_torch", "redstone_wall_torch", "redstone_lamp", "repeater", "comparator",
    "lever", "daylight_detector", "observer", "piston", "sticky_piston", "dispenser", "dropper", "hopper",
    "target", "tripwire", "tripwire_hook", "sculk_sensor", "calibrated_sculk_sensor", "crafter"
}
PLANT_PREFIXES = ("potted_",)
PLANT_NAMES = {
    "grass", "tall_grass", "fern", "large_fern", "dead_bush", "dandelion", "poppy", "blue_orchid",
    "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip", "oxeye_daisy",
    "cornflower", "lily_of_the_valley", "wither_rose", "sunflower", "lilac", "rose_bush", "peony",
    "sugar_cane", "cactus", "kelp", "seagrass", "tall_seagrass", "vine", "glow_lichen", "torchflower",
    "pitcher_plant", "small_dripleaf", "big_dripleaf", "spore_blossom", "moss_carpet", "firefly_bush"
}

def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))

def stable_json(data) -> bytes:
    return (json.dumps(data, ensure_ascii=False, indent=2, sort_keys=False) + "\n").encode("utf-8")

def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def expanded_states(block):
    states = []
    for state in block.get("states", []):
        state = dict(state)
        # The source schema omits boolean values, but the bool type and cardinality
        # define them exactly; make the catalogue schema explicit for consumers.
        if state.get("type") == "bool" and "values" not in state:
            state["values"] = [False, True]
        states.append(state)
    return states

def as_state_shape_ids(entry, block):
    if isinstance(entry, int):
        return [entry] * (block["maxStateId"] - block["minStateId"] + 1)
    if isinstance(entry, list):
        return entry
    return None

def derive_geometry_classifications(block, state_shapes, shape_boxes, official_blocks, models):
    """Return exactly one geometry class, sourced from official model geometry."""
    name = block["name"]
    official = official_blocks.get("minecraft:" + name, {})
    kind = official.get("definition", {}).get("type", "").split(":")[-1]
    blockstate = models["blockstates"].get(name)
    refs = set()
    def collect(value):
        if isinstance(value, dict):
            if isinstance(value.get("model"), str): refs.add(value["model"])
            for child in value.values(): collect(child)
        elif isinstance(value, list):
            for child in value: collect(child)
    if blockstate: collect(blockstate)
    element_sets = [models["models"][ref]["resolved_elements"] for ref in refs if ref in models["models"]]
    has_geometry = any(element_sets)
    full = bool(element_sets) and all(any(e.get("from") == [0, 0, 0] and e.get("to") == [16, 16, 16] for e in elements) for elements in element_sets)
    if kind == "liquid": geometry = "fluid"
    elif name in MULTIPART_NAMES and name != "sulfur_spike": geometry = "multipart_structure"
    elif full or name in {"brown_mushroom_block", "red_mushroom_block", "mushroom_stem"}: geometry = "full_cube"
    elif not has_geometry: geometry = "non_solid"
    else: geometry = "partial_geometry"
    return [geometry]

def collision_kind(boxes):
    if boxes == []: return "empty"
    if boxes == [[0.0, 0.0, 0.0, 1.0, 1.0, 1.0]]: return "full"
    if boxes is None: return "unknown"
    if len(boxes) > 1: return "compound"
    return "partial"

def build(source: Path):
    blocks = read_json(source / "blocks.json")
    collision = read_json(source / "blockCollisionShapes.json")
    materials = read_json(source / "materials.json")
    items = read_json(source / "items.json")
    official_dir = Path(__file__).resolve().parent / "source" / "official_26.2"
    official_blocks = read_json(official_dir / "blocks.json")
    client_models = read_json(official_dir / "client_geometry.json")
    shape_boxes = collision["shapes"]
    item_by_name = {item["name"]: item for item in items}
    source_files = ["blocks.json", "blockCollisionShapes.json", "materials.json", "items.json"]
    manifest = {
        "repository": SOURCE_REPO, "commit": SOURCE_COMMIT, "data_path": SOURCE_DATA_PATH,
        "edition": "Minecraft Java Edition", "version": "26.2",
        "files": {name: {"sha256": sha256(source / name), "bytes": (source / name).stat().st_size} for name in source_files}
    }
    definitions, behaviour, mappings = [], [], {}
    unresolved = {"render_geometry": [], "selection_geometry": [], "block_entity_type": [], "block_item_mapping": [],
                  "interaction_details": [], "support_rules": [], "neighbour_dependent_collision": [],
                  "translucency": [], "flammability": []}
    counts = {"classifications": {}, "behaviour_tags": {}, "state_properties": 0, "blocks_with_state_properties": 0,
              "state_count": 0, "collision_shapes_referenced": 0, "blocks_with_collision_mapping": 0,
              "collision_state_mappings": 0, "blocks_without_matching_item": 0, "blocks_with_light": 0,
              "blocks_with_block_entity_metadata": 0}
    used_shapes = set()
    all_names = {b["name"] for b in blocks}
    for block in blocks:
        name = block["name"]
        shape_entry = collision["blocks"].get(name)
        state_shape_ids = as_state_shape_ids(shape_entry, block) if shape_entry is not None else None
        state_count = block["maxStateId"] - block["minStateId"] + 1
        if state_shape_ids is not None and len(state_shape_ids) == state_count:
            state_mapping = {str(block["minStateId"] + i): sid for i, sid in enumerate(state_shape_ids)}
            mappings[f"minecraft:{name}"] = state_mapping
            used_shapes.update(state_shape_ids)
            counts["blocks_with_collision_mapping"] += 1
            counts["collision_state_mappings"] += len(state_mapping)
        else:
            state_mapping = None
            mappings[f"minecraft:{name}"] = None
        classifications = derive_geometry_classifications(block, state_shape_ids, shape_boxes, official_blocks, client_models)
        tags = set()
        shape_lists = [shape_boxes.get(str(sid)) for sid in (state_shape_ids or [])]
        valid_shapes = [boxes for boxes in shape_lists if boxes is not None]
        state_collision_kinds = [collision_kind(boxes) for boxes in shape_lists]
        default_boxes = (shape_boxes.get(str(state_shape_ids[block["defaultState"] - block["minStateId"]]))
                         if state_shape_ids else None)
        if "full" in state_collision_kinds: tags.add("full_collision")
        if "partial" in state_collision_kinds: tags.add("partial_collision")
        if "empty" in state_collision_kinds: tags.add("empty_collision")
        if "compound" in state_collision_kinds: tags.add("compound_collision")
        if block.get("transparent") is True: tags.add("transparent")
        if block.get("emitLight", 0) > 0: tags.add("light_emitter")
        if any(prop.get("name") == "waterlogged" for prop in block.get("states", [])): tags.add("waterloggable")
        if name in GRAVITY_NAMES or name.endswith("_concrete_powder"): tags.add("gravity_affected")
        if name in MULTIPART_NAMES or name == "sulfur_spike": tags.add("multi_block")
        if name in INTERACTIVE_NAMES or name.endswith("_shulker_box"): tags.add("interactive")
        if name in REDSTONE_NAMES or name.endswith("_button") or name.endswith("_pressure_plate"): tags.add("redstone_component")
        if name in PLANT_NAMES or name.startswith(PLANT_PREFIXES): tags.add("plant")
        if name in {"water", "lava", "fire", "soul_fire", "nether_portal", "end_portal", "campfire", "soul_campfire", "bubble_column", "potent_sulfur"}: tags.add("animated")
        if name in {"ice", "packed_ice", "blue_ice", "frosted_ice", "slime_block", "honey_block"}: tags.add("slippery")
        if name in {"slime_block", "bed"} or name.endswith("_bed"): tags.add("bouncy")
        if name in {"magma_block", "cactus", "sweet_berry_bush", "wither_rose", "fire", "soul_fire", "campfire", "soul_campfire", "sulfur_spike"}: tags.add("damaging")
        if name in {"barrier", "light", "structure_void", "structure_block", "jigsaw", "command_block", "chain_command_block", "repeating_command_block", "end_gateway", "end_portal", "nether_portal", "moving_piston", "piston_head", "spawner", "trial_spawner", "vault"}: tags.add("special_rendering")
        if name in {"torch", "wall_torch", "soul_torch", "soul_wall_torch", "redstone_torch", "redstone_wall_torch", "lever", "stone_button", "oak_button", "rail", "powered_rail", "detector_rail", "activator_rail", "sulfur_spike"} or name.endswith("_sapling"):
            tags.add("support_dependent")
        if "block_entity" in block:
            tags.add("block_entity")
        item = item_by_name.get(name)
        if item is None: counts["blocks_without_matching_item"] += 1
        materials_for_block = materials.get(block.get("material"))
        dims = None
        if state_shape_ids:
            boxes = shape_boxes.get(str(state_shape_ids[block["defaultState"] - block["minStateId"]]), [])
            if boxes:
                dims = [min(box[i] for box in boxes) for i in range(3)] + [max(box[i] for box in boxes) for i in range(3, 6)]
        definition = {
            "registry": {"id": block["id"], "identifier": f"minecraft:{name}", "internal_name": name,
                         "display_name": block.get("displayName"), "min_state_id": block["minStateId"],
                         "max_state_id": block["maxStateId"], "default_state_id": block["defaultState"],
                         "state_count": state_count, "states": expanded_states(block)},
            "classification": {"physical": classifications, "primary_geometry": next((c for c in ["technical", "fluid", "multipart_structure", "full_cube", "partial_geometry", "non_solid"] if c in classifications), "non_solid")},
            "properties": {"hardness": block.get("hardness"), "blast_resistance": block.get("resistance"),
                           "material": block.get("material"), "transparent": block.get("transparent"),
                           "translucent": None, "light_filter": block.get("filterLight"), "light_emission": block.get("emitLight"),
                           "diggable": block.get("diggable"), "harvest_tools": block.get("harvestTools"),
                           "material_tool_speeds": materials_for_block},
            "geometry": {"geometry_type": block.get("boundingBox"), "render_geometry": None,
                         "dimensions_default_state": dims, "dimensions_source": "collision_bounds_only" if dims is not None else "unknown",
                         "full_cube": None, "partial_geometry": None,
                         "multipart_geometry": name in MULTIPART_NAMES, "neighbour_dependent": "unknown"},
            "collision": {"shape_source": f"minecraft:{name}" if state_mapping is not None else None,
                          "default_state_shape_id": state_shape_ids[block["defaultState"] - block["minStateId"]] if state_shape_ids else None,
                          "default_state_boxes": default_boxes,
                          "default_state_box_count": len(default_boxes) if default_boxes is not None else None,
                          "default_state_kind": collision_kind(default_boxes),
                          "state_collision_kind_counts": {kind: state_collision_kinds.count(kind) for kind in ["empty", "full", "partial", "compound", "unknown"] if kind in state_collision_kinds},
                          "state_dependent": len(set(state_shape_ids or [])) > 1,
                          "neighbour_dependent": "unknown", "selection_shape": "requires_further_investigation"},
            "item": {"id": item.get("id"), "identifier": f"minecraft:{item['name']}"} if item else None,
            "source_fields": {key: block[key] for key in ("drops", "boundingBox") if key in block}
        }
        definitions.append(definition)
        beh = {"identifier": f"minecraft:{name}", "tags": sorted(tags),
               "gravity": {"affected": "gravity_affected" in tags, "falling_or_breaking_rule": "requires_version_specific_rule_lookup" if "gravity_affected" in tags else False},
               "support": {"dependent": True if "support_dependent" in tags else None,
                           "requirements": "requires_version_specific_rule_lookup" if "support_dependent" in tags else "unknown"},
               "lighting": {"emission": block.get("emitLight"), "filter": block.get("filterLight")},
               "waterlogging": any(prop.get("name") == "waterlogged" for prop in block.get("states", [])),
               "interaction": "requires_version_specific_rule_lookup" if "interactive" in tags else "unknown",
               "flammability": None,
               "special_physics": [tag for tag in ("slippery", "bouncy", "damaging") if tag in tags],
               "multipart": {"is_multi_block": True if "multi_block" in tags else None,
                             "part_properties": [p for p in block.get("states", []) if p.get("name") in {"part", "half", "face", "vertical_direction"}],
                             "relationship_details": "requires_further_investigation" if "multi_block" in tags else "unknown"},
               "block_entity": {"status": "known_absent_beds_use_head_foot_states" if name.endswith("_bed") else "source_does_not_provide_block_entity_registry", "type": None},
               "special_mechanics": ({"mechanic": "potent_sulfur", "source": "Minecraft Java Edition 26.2 release notes",
                  "properties": ["potent_sulfur_state: dry|wet|dormant|erupting|continuous"],
                  "details": ["Generates bubbles through up to 4 water source blocks above.",
                    "With up to 4 source water blocks above, produces nausea-causing gas on the water surface; gas spreads through adjacent water to radius 3.",
                    "Magma beneath plus 1–4 source water blocks forms a geyser with randomized eruptions and upward entity impulse.",
                    "Lava beneath plus 1–4 source water blocks forms a continuous geyser with upward entity impulse.",
                    "Geyser start and stop emit game events detectable by sculk sensors."]} if name == "potent_sulfur" else
                  {"mechanic": "sulfur_spike", "source": "Minecraft Java Edition 26.2 Snapshot 3 and release notes",
                   "properties": ["thickness", "vertical_direction", "waterlogged"],
                   "details": ["Forms stalactites from ceilings and stalagmites from floors.",
                     "Adjacent tips merge unless placement uses crouch.",
                     "Unsupported stalagmites break; unsupported stalactites fall.",
                     "A falling stalactite damages entities it hits."]} if name == "sulfur_spike" else None)}
        behaviour.append(beh)
        if definition["geometry"]["render_geometry"] is None: unresolved["render_geometry"].append(f"minecraft:{name}")
        unresolved["selection_geometry"].append(f"minecraft:{name}")
        if not name.endswith("_bed"):
            unresolved["block_entity_type"].append(f"minecraft:{name}")
        if item is None: unresolved["block_item_mapping"].append(f"minecraft:{name}")
        unresolved["interaction_details"].append(f"minecraft:{name}")
        unresolved["support_rules"].append(f"minecraft:{name}")
        unresolved["neighbour_dependent_collision"].append(f"minecraft:{name}")
        unresolved["translucency"].append(f"minecraft:{name}")
        unresolved["flammability"].append(f"minecraft:{name}")
        counts["state_count"] += state_count
        counts["state_properties"] += len(block.get("states", []))
        counts["blocks_with_state_properties"] += bool(block.get("states"))
        counts["blocks_with_light"] += block.get("emitLight", 0) > 0
        for c in classifications: counts["classifications"][c] = counts["classifications"].get(c, 0) + 1
        for tag in tags: counts["behaviour_tags"][tag] = counts["behaviour_tags"].get(tag, 0) + 1
    source_count = len(blocks)
    expected_26_2 = {"sulfur", "potent_sulfur", "sulfur_slab", "sulfur_stairs", "sulfur_wall", "polished_sulfur",
        "polished_sulfur_slab", "polished_sulfur_stairs", "polished_sulfur_wall", "sulfur_bricks", "sulfur_brick_slab",
        "sulfur_brick_stairs", "sulfur_brick_wall", "chiseled_sulfur", "cinnabar", "cinnabar_slab", "cinnabar_stairs",
        "cinnabar_wall", "polished_cinnabar", "polished_cinnabar_slab", "polished_cinnabar_stairs", "polished_cinnabar_wall",
        "cinnabar_bricks", "cinnabar_brick_slab", "cinnabar_brick_stairs", "cinnabar_brick_wall", "chiseled_cinnabar", "sulfur_spike"}
    catalogue = {"schema_version": 1, "source": manifest, "expected_source_registry_count": source_count,
                 "classification_values": sorted(CLASSIFICATIONS), "behaviour_tag_values": sorted(BEHAVIOUR_TAGS), "blocks": definitions}
    categories = {"schema_version": 1, "physical_classifications": {key: [] for key in sorted(CLASSIFICATIONS)},
                  "behaviour_tags": {key: [] for key in sorted(BEHAVIOUR_TAGS)}}
    for definition, beh in zip(definitions, behaviour):
        identifier = definition["registry"]["identifier"]
        for key in definition["classification"]["physical"]: categories["physical_classifications"][key].append(identifier)
        for key in beh["tags"]: categories["behaviour_tags"][key].append(identifier)
    collision_output = {"schema_version": 1, "coordinate_order": ["x_min", "y_min", "z_min", "x_max", "y_max", "z_max"],
                        "coordinate_space": "block_local; raw values preserved", "shapes": shape_boxes,
                        "block_state_mappings": mappings}
    audit = {"errors": [], "warnings": [], "unresolved": {k: {"count": len(v), "identifiers": v} for k, v in unresolved.items()},
             "source_registry_count": source_count, "catalogued_count": len(definitions),
             "source_names_missing_from_catalogue": sorted(all_names - {b["registry"]["internal_name"] for b in definitions}),
             "catalogue_names_missing_from_source": sorted({b["registry"]["internal_name"] for b in definitions} - all_names),
             "source_26_2_additions": sorted(expected_26_2), "missing_26_2_additions": sorted(expected_26_2 - all_names)}
    audit["warnings"].append("Render model geometry is not present in the pinned source; all entries require a separate model-data source.")
    audit["warnings"].append("Selection/raycast shapes are not present in the pinned source; all entries require further investigation.")
    audit["warnings"].append("Block entity types, exact interaction/support rules, flammability, and translucency are not provided by the pinned registry datasets and remain unresolved.")
    stats = {"source_registry_count": source_count, "catalogued_count": len(definitions), "coverage_percent": round(100 * len(definitions) / source_count, 4) if source_count else 0,
             **counts, "classifications": dict(sorted(counts["classifications"].items())),
             "behaviour_tags": dict(sorted(counts["behaviour_tags"].items())),
             "collision_shape_catalog_count": len(shape_boxes), "collision_shapes_referenced": len(used_shapes),
             "collision_shape_coverage_percent": round(100 * counts["blocks_with_collision_mapping"] / source_count, 4) if source_count else 0,
             "source_names_missing_from_catalogue": len(audit["source_names_missing_from_catalogue"]),
             "missing_26_2_additions": audit["missing_26_2_additions"], "unresolved_field_counts": {k: len(v) for k, v in unresolved.items()},
             "source_commit": SOURCE_COMMIT}
    output = {"blocks_master.json": catalogue, "blocks_by_category.json": categories, "collision_shapes.json": collision_output,
              "block_behaviours.json": {"schema_version": 1, "blocks": behaviour},
              "catalogue_statistics.json": stats, "catalogue_audit.json": audit,
              "source_manifest.json": manifest}
    return enrich_outputs(output, source)


OFFICIAL_SOURCE = "https://www.minecraft.net/en-us/article/minecraft-java-edition-26-2"
BLOCK_ENTITY_ALIASES = {
    "banner": ["banner", "wall_banner"], "brushable_block": ["brushable"],
    "chest": ["chest", "copper_chest", "weathering_copper_chest"],
    "chiseled_bookshelf": ["chiseled_book_shelf"], "command_block": ["command"],
    "copper_golem_statue": ["copper_golem_statue", "weathering_copper_golem_statue"],
    "enchanting_table": ["enchantment_table"], "hanging_sign": ["ceiling_hanging_sign", "wall_hanging_sign"],
    "mob_spawner": ["spawner"], "piston": ["moving_piston"],
    "sign": ["standing_sign", "wall_sign"],
    "skull": ["skull", "wall_skull", "wither_skull", "wither_wall_skull", "player_head", "player_wall_head", "piglinwallskull"],
    "structure_block": ["structure"], "test_block": ["test"], "test_instance_block": ["test_instance"]
}
MULTIPART_NAMES.discard("sulfur_spike")

def enrich_outputs(output, source):
    """Join pinned upstream data to official 26.2 server and client evidence."""
    official_dir = Path(__file__).resolve().parent / "source" / "official_26.2"
    official_blocks = read_json(official_dir / "blocks.json")
    official_registries = read_json(official_dir / "registries.json")
    item_block_components = read_json(official_dir / "item_block_state_components.json")
    models = read_json(official_dir / "client_geometry.json")
    entity_types = set(official_registries["minecraft:block_entity_type"]["entries"])
    kind_to_entity = {}
    for entity in entity_types:
        kind = entity.split(":", 1)[1]
        for alias in BLOCK_ENTITY_ALIASES.get(kind, [kind]): kind_to_entity[alias] = entity
    for entity, aliases in BLOCK_ENTITY_ALIASES.items():
        for alias in aliases: kind_to_entity[alias] = "minecraft:" + entity
    state_assets = models["blockstates"]
    model_assets = models["models"]
    original = output["blocks_master.json"]["blocks"]
    behavior_by_name = {b["identifier"].split(":",1)[1]: b for b in output["block_behaviours.json"]["blocks"]}
    geometry_counts = {}
    unresolved = {k: [] for k in ("selection_geometry", "interaction_details", "support_rules", "neighbour_dependent_collision", "translucency", "flammability")}
    be_unresolved=[]; render_unresolved=[]; item_unresolved=[]
    item_report=[]
    for entry in original:
        name=entry["registry"]["internal_name"]; ident="minecraft:"+name
        official=official_blocks.get(ident, {})
        typ=official.get("definition",{}).get("type","").split(":")[-1]
        bs=state_assets.get(name)
        refs=set()
        def collect(v):
            if isinstance(v,dict):
                if isinstance(v.get("model"),str): refs.add(v["model"])
                for x in v.values(): collect(x)
            elif isinstance(v,list):
                for x in v: collect(x)
        if bs: collect(bs)
        element_sets=[model_assets[r]["resolved_elements"] for r in sorted(refs) if r in model_assets]
        full_element={"from":[0,0,0],"to":[16,16,16]}
        has_geometry=any(elements for elements in element_sets)
        is_full=bool(element_sets) and all(any(e.get("from")==[0,0,0] and e.get("to")==[16,16,16] for e in es) for es in element_sets)
        if typ=="liquid": geom="fluid"
        elif name in MULTIPART_NAMES: geom="multipart_structure"
        elif is_full or name in {"brown_mushroom_block","red_mushroom_block","mushroom_stem"}: geom="full_cube"
        elif not has_geometry: geom="non_solid"
        else: geom="partial_geometry"
        geometry_counts[geom]=geometry_counts.get(geom,0)+1
        entry["classification"]={"primary_geometry":geom}
        entry["geometry"].update({"render_geometry":"model_elements" if bs else None,"render_model_count":len(refs),"render_model_identifiers":sorted(refs),"render_geometry_class":geom,"render_geometry_source":"official_26.2_client_blockstate_and_model_assets" if bs else None,"full_cube":geom=="full_cube","partial_geometry":geom=="partial_geometry","multipart_geometry":geom=="multipart_structure","neighbour_dependent":"unknown"})
        entry["collision"]["selection_shape"]="unknown_requires_outline_shape_extraction"
        beh=behavior_by_name[name]
        tags=set(beh["tags"])
        if geom=="fluid": tags.add("fluid")
        if name in TECHNICAL_NAMES or name.endswith("_command_block"): tags.add("technical")
        beh["tags"]=sorted(tags)
        entity=kind_to_entity.get(typ)
        be_status="mapped" if entity else "known_absent"
        beh["block_entity"]={"status":be_status,"type":entity,"official_block_kind":official.get("definition",{}).get("type")}
        entry["block_entity"]={"status":be_status,"type":entity}
        if not bs: render_unresolved.append(ident)
        for field in unresolved: unresolved[field].append(ident)

        if entry["item"] is None:
            item_unresolved.append(ident)
            drops=entry.get("source_fields",{}).get("drops",[])
            item_report.append({"block":ident,"status":"no_exact_name_item","official_block_kind":official.get("definition",{}).get("type"),"drop_candidates":drops,"relationship":"unverified" if drops else "no_source_mapping","candidate_class":"differently_named_item_candidate" if drops else ("technical_or_state_only_candidate" if typ in {"air","liquid","flower_pot","candle_cake","wall_banner","wall_sign","wall_hanging_sign","wall_skull","wall_torch","attached_stem","stem","piston_head","moving_piston","end_portal","end_gateway","nether_portal","redstone_wire","tripwire","fire","soul_fire","bubble_column"} else "missing_source_mapping"),"reason":"The source item registry has no exact-name entry; drop candidates do not prove a placeable block item."})
        else:
            item_report.append({"block":ident,"status":"exact_name_item","item":entry["item"]["identifier"],"relationship":"exact_identifier_match"})
    # Preserve a one-class invariant and rebuild category membership by dimension.
    cats={k:[] for k in sorted(GEOMETRY_CLASSES)}
    tags_catalog={k:[] for k in sorted(BEHAVIOUR_TAGS|{"technical","fluid"})}
    for e in original:
        ident=e["registry"]["identifier"]; cats[e["classification"]["primary_geometry"]].append(ident)
    for b in output["block_behaviours.json"]["blocks"]:
        for tag in b["tags"]: tags_catalog.setdefault(tag,[]).append(b["identifier"])
    output["blocks_by_category.json"]={"schema_version":2,"geometry_classes":cats,"behaviour_tags":tags_catalog}
    output["blocks_master.json"]["schema_version"]=2
    output["blocks_master.json"]["classification_values"]=sorted(GEOMETRY_CLASSES)
    output["block_behaviours.json"]["schema_version"]=2
    output["block_behaviours.json"]["blocks"]=output["block_behaviours.json"]["blocks"]
    # Emit authoritative item relationship coverage; unresolved candidates are never promoted to links.
    output["block_item_relationships.json"]={"schema_version":1,"source":"pinned Complexity-ML item registry and official 26.2 block definitions","obtainability_conclusion":"The supplied official registry/component reports cannot prove that any no-exact-name block is obtainable or unobtainable as an inventory item. Candidate classes are investigative leads, not conclusions.","official_item_block_state_components":item_block_components,"counts":{"exact_name_items":len(original)-len(item_unresolved),"without_exact_name_item":len(item_unresolved),"with_unverified_drop_candidates":sum(bool(x.get("drop_candidates")) for x in item_report if x["status"]=="no_exact_name_item"),"without_source_mapping":sum(x.get("candidate_class")=="missing_source_mapping" for x in item_report if x["status"]=="no_exact_name_item"),"technical_or_state_only_candidates":sum(x.get("candidate_class")=="technical_or_state_only_candidate" for x in item_report if x["status"]=="no_exact_name_item"),"differently_named_item_candidates":sum(x.get("candidate_class")=="differently_named_item_candidate" for x in item_report if x["status"]=="no_exact_name_item")},"blocks":item_report}
    # Detailed unresolved-field accounting states what the available sources can and cannot prove.
    unresolved_defs={
      "render_geometry":{"count":len(render_unresolved),"identifiers":render_unresolved,"reason":"No official blockstate model file was found for this registry entry.","source_dataset_available":True,"authoritative_source":"official 26.2 client assets"},
      "selection_geometry":{"count":len(original),"identifiers":[e["registry"]["identifier"] for e in original],"reason":"The server block report and client models do not expose outline/raycast shape behavior; requires version-specific runtime/code extraction.","source_dataset_available":False,"authoritative_source":"Mojang server implementation/runtime required"},
      "block_entity_type":{"count":len(be_unresolved),"identifiers":be_unresolved,"reason":"Block kind did not map to the official block_entity_type registry after documented aliases.","source_dataset_available":True,"authoritative_source":"official 26.2 server reports"},
      "block_item_mapping":{"count":len(item_unresolved),"identifiers":item_unresolved,"reason":"No exact-name item; candidate drops and inferred aliases do not prove a placeable block item. See block_item_relationships.json for candidate classes.","source_dataset_available":False,"authoritative_source":"Official item registry has item identities but no explicit block-to-item mapping"},
      "interaction_details":{"count":len(original),"identifiers":[e["registry"]["identifier"] for e in original],"reason":"State definitions do not encode use/click logic.","source_dataset_available":False,"authoritative_source":"Mojang version-specific block behavior code/runtime required"},
      "support_rules":{"count":len(original),"identifiers":[e["registry"]["identifier"] for e in original],"reason":"Registry states and collision maps do not encode placement validation or neighbor-update rules.","source_dataset_available":False,"authoritative_source":"Mojang version-specific block behavior code/runtime required"},
      "neighbour_dependent_collision":{"count":len(original),"identifiers":[e["registry"]["identifier"] for e in original],"reason":"State-specific collision is available; whether updates depend on neighbor changes is not represented in these datasets.","source_dataset_available":False,"authoritative_source":"Mojang runtime behavior required"},
      "translucency":{"count":len(original),"identifiers":[e["registry"]["identifier"] for e in original],"reason":"The registry transparent flag is not the renderer's alpha/translucency or render-layer classification.","source_dataset_available":False,"authoritative_source":"Client texture alpha plus render pipeline/code required"},
      "flammability":{"count":len(original),"identifiers":[e["registry"]["identifier"] for e in original],"reason":"No fire-spread flammability field is included in the registry, server block report, or client models.","source_dataset_available":False,"authoritative_source":"Mojang version-specific fire behavior code required"}}
    # Add exact block entity coverage to audit and rebuild schema-versioned outputs.
    audit=output["catalogue_audit.json"]
    audit.update({"schema_version":2,"unresolved":unresolved_defs,"classification_consistency":{"geometry_classes":geometry_counts,"blocks_with_exactly_one_primary_geometry":len(original),"behavior_is_independent":True},"block_entity_summary":{"mapped_blocks":sum(1 for e in original if e["block_entity"]["type"]),"known_absent":sum(1 for e in original if not e["block_entity"]["type"]),"known_bed_absent":sum(1 for e in original if e["registry"]["internal_name"].endswith("_bed")),"unresolved":len(be_unresolved),"official_entity_types":len(entity_types)},"item_relationship_summary":output["block_item_relationships.json"]["counts"],"source_comparisons":{"official_block_registry_count":len(official_blocks),"client_blockstate_count":len(state_assets),"official_block_entity_type_count":len(entity_types),"official_block_item_registry_count":len(official_registries["minecraft:item"]["entries"]),"official_item_block_state_component_count":len(item_block_components),"official_server_url":OFFICIAL_SOURCE}})
    audit["warnings"]=["Model geometry describes blockstate model element bounds; it does not prove in-game render layer, transparency, selection shape, or animation.","Unknown support, interaction, neighbor-update, translucency, and flammability fields require behavior/runtime sources and are not interpreted as false."]
    stats=output["catalogue_statistics.json"]
    stats["schema_version"]=2; stats["geometry_class_counts"]=geometry_counts
    stats["blocks_with_one_geometry_class"]=len(original); stats["blocks_without_matching_item"]=len(item_unresolved)
    stats["unresolved_field_counts"]={k:v["count"] for k,v in unresolved_defs.items()}
    output["catalogue_statistics.json"]=stats
    # retain normalized source manifest and append official provenance hashes
    manifest=output["source_manifest.json"]
    manifest["official_26_2"]={"server_reports":{"blocks_sha256":sha256(official_dir/"blocks.json"),"registries_sha256":sha256(official_dir/"registries.json"),"item_block_state_components_sha256":sha256(official_dir/"item_block_state_components.json"),"server_jar_sha1":"823e2250d24b3ddac457a60c92a6a941943fcd6a","generated_by":"Mojang net.minecraft.data.Main --reports --output"},"client_geometry_sha256":sha256(official_dir/"client_geometry.json"),"client_jar_sha1":models["source"]["client_jar_sha1"],"extractor":"tools/catalogue/extract_mojang_client_geometry.py"}
    output["source_manifest.json"]=manifest
    output["catalogue_audit.json"]=audit
    return output

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    outputs = build(args.source_dir)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        (args.output_dir / name).write_bytes(stable_json(data))
    print(f"Generated {len(outputs)} deterministic files for {outputs['catalogue_statistics.json']['catalogued_count']} blocks at {args.output_dir}")
if __name__ == "__main__": main()
