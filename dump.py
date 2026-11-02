import json

with open("app/src/main/assets/atlas_map.json") as f:
    atlasMap = json.load(f)

def getTex(name):
    return atlasMap.get(name, 0)

preset_ids = {
    "grass_block": 1, "dirt": 2, "stone": 3, "oak_log": 4,
    "oak_leaves": 5, "water": 6, "oak_planks": 7, "sand": 8,
    "glass": 9, "bricks": 10, "gravel": 11, "coal_ore": 12,
    "iron_ore": 13, "diamond_ore": 14, "cobblestone": 15
}

with open("app/src/main/assets/blocks.json") as f:
    j = json.load(f)

fc_id = 16
for mc_id, el in j.items():
    if not mc_id.startswith("minecraft:"): continue
    identifier = mc_id[10:]
    
    block_id = fc_id
    if identifier in preset_ids:
        block_id = preset_ids[identifier]
    else:
        fc_id += 1
        
    texTop = getTex(identifier + "_top")
    texBottom = getTex(identifier + "_bottom")
    texSide = getTex(identifier + "_side")
    texAll = getTex(identifier)
    
    if block_id <= 15:
        print(f"ID {block_id} ({identifier}): all={texAll} top={texTop} bot={texBottom} side={texSide}")

