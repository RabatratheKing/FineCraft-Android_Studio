#include "Registry.h"
#include "../core/Logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>

std::unordered_map<uint16_t, BlockDefinition> Registry::blocks;
std::unordered_map<std::string, uint16_t> Registry::identifierToBlockId;
std::unordered_map<int32_t, uint16_t> Registry::stateIdToBlockId;
std::unordered_map<int32_t, int32_t> Registry::stateIdToShapeId;
std::unordered_map<int32_t, std::vector<AABB>> Registry::shapes;
std::unordered_map<uint16_t, ItemDefinition> Registry::items;
bool Registry::initialized = false;

const BlockDefinition Registry::fallbackBlock = {0, "minecraft:air", "air", "Air", 0, 0, 0, GeometryClass::NON_SOLID, false, false, false, 0.0f, 0, 0, 0, 0, 0, 0};
const ItemDefinition Registry::fallbackItem = {0, "minecraft:air", "Air", 64, 0, 0};
const std::vector<AABB> Registry::fullCubeBoxes = { {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f} };
const std::vector<AABB> Registry::emptyBoxes = {};

void Registry::init() {
    if (initialized) return;
    blocks.clear();
    identifierToBlockId.clear();
    stateIdToBlockId.clear();
    stateIdToShapeId.clear();
    shapes.clear();
    items.clear();

    // ID 0: Air
    BlockDefinition air = {0, "minecraft:air", "air", "Air", 0, 0, 0, GeometryClass::NON_SOLID, false, false, false, 0.0f, 0, 0, 0, 0, 0, 0};
    blocks[0] = air;
    identifierToBlockId["minecraft:air"] = 0;
    identifierToBlockId["air"] = 0;
    stateIdToBlockId[0] = 0;

    // Standard default block textures mapping for core prototype voxel types
    // Grass (1)
    BlockDefinition grass = {1, "minecraft:grass_block", "grass_block", "Grass Block", 1, 1, 1, GeometryClass::FULL_CUBE, true, true, true, 0.6f, 0, 0, 2, 3, 3, 3};
    blocks[1] = grass;
    identifierToBlockId["minecraft:grass_block"] = 1;
    identifierToBlockId["grass_block"] = 1;

    // Dirt (2)
    BlockDefinition dirt = {2, "minecraft:dirt", "dirt", "Dirt", 2, 2, 2, GeometryClass::FULL_CUBE, true, true, true, 0.5f, 0, 2, 2, 2, 2, 2};
    blocks[2] = dirt;
    identifierToBlockId["minecraft:dirt"] = 2;
    identifierToBlockId["dirt"] = 2;

    // Stone (3)
    BlockDefinition stone = {3, "minecraft:stone", "stone", "Stone", 1, 1, 1, GeometryClass::FULL_CUBE, true, true, true, 1.5f, 0, 1, 1, 1, 1, 1};
    blocks[3] = stone;
    identifierToBlockId["minecraft:stone"] = 3;
    identifierToBlockId["stone"] = 3;

    // Oak Stairs (4)
    BlockDefinition stairs = {4, "minecraft:oak_stairs", "oak_stairs", "Oak Stairs", 3918, 3997, 3918, GeometryClass::PARTIAL_GEOMETRY, true, false, true, 2.0f, 0, 4, 4, 4, 4, 4};
    blocks[4] = stairs;
    identifierToBlockId["minecraft:oak_stairs"] = 4;
    identifierToBlockId["oak_stairs"] = 4;

    // Torch (5)
    BlockDefinition torch = {5, "minecraft:torch", "torch", "Torch", 3370, 3370, 3370, GeometryClass::PARTIAL_GEOMETRY, false, false, true, 0.0f, 14, 5, 5, 5, 5, 5};
    blocks[5] = torch;
    identifierToBlockId["minecraft:torch"] = 5;
    identifierToBlockId["torch"] = 5;

    // Oak Door (6)
    BlockDefinition door = {6, "minecraft:oak_door", "oak_door", "Oak Door", 5658, 5721, 5666, GeometryClass::MULTIPART_STRUCTURE, true, false, true, 3.0f, 0, 6, 6, 6, 6, 6};
    blocks[6] = door;
    identifierToBlockId["minecraft:oak_door"] = 6;
    identifierToBlockId["oak_door"] = 6;

    initialized = true;
}

void Registry::loadCatalogueFromAssets(AAssetManager* assetManager) {
    if (!assetManager) {
        LOGE("AAssetManager is null in loadCatalogueFromAssets");
        return;
    }
    init();

    AAsset* masterAsset = AAssetManager_open(assetManager, "catalogue/blocks_master.json", AASSET_MODE_BUFFER);
    AAsset* shapeAsset = AAssetManager_open(assetManager, "catalogue/collision_shapes.json", AASSET_MODE_BUFFER);

    if (!masterAsset || !shapeAsset) {
        LOGE("Failed to open catalogue asset files from AAssetManager");
        if (masterAsset) AAsset_close(masterAsset);
        if (shapeAsset) AAsset_close(shapeAsset);
        return;
    }

    std::string masterContent((const char*)AAsset_getBuffer(masterAsset), AAsset_getLength(masterAsset));
    std::string shapeContent((const char*)AAsset_getBuffer(shapeAsset), AAsset_getLength(shapeAsset));

    AAsset_close(masterAsset);
    AAsset_close(shapeAsset);

    std::stringstream masterSs(masterContent);
    std::stringstream shapeSs(shapeContent);

    loadCatalogueFromStreams(masterSs, shapeSs);
}

void Registry::loadCatalogueFromStreams(std::istream& blocksMasterStream, std::istream& collisionShapesStream) {
    try {
        nlohmann::json masterJson = nlohmann::json::parse(blocksMasterStream);
        nlohmann::json shapesJson = nlohmann::json::parse(collisionShapesStream);

        // Parse collision shapes
        if (shapesJson.contains("shapes") && shapesJson["shapes"].is_object()) {
            for (auto& [shapeIdStr, boxArray] : shapesJson["shapes"].items()) {
                int32_t shapeId = std::stoi(shapeIdStr);
                std::vector<AABB> boxList;
                if (boxArray.is_array()) {
                    for (auto& boxJson : boxArray) {
                        if (boxJson.is_array() && boxJson.size() == 6) {
                            AABB box;
                            box.minX = boxJson[0].get<float>();
                            box.minY = boxJson[1].get<float>();
                            box.minZ = boxJson[2].get<float>();
                            box.maxX = boxJson[3].get<float>();
                            box.maxY = boxJson[4].get<float>();
                            box.maxZ = boxJson[5].get<float>();
                            boxList.push_back(box);
                        }
                    }
                }
                shapes[shapeId] = boxList;
            }
        }

        if (shapesJson.contains("block_state_mappings") && shapesJson["block_state_mappings"].is_object()) {
            for (auto& [blockIdent, stateMap] : shapesJson["block_state_mappings"].items()) {
                if (stateMap.is_object()) {
                    for (auto& [stateIdStr, shapeIdVal] : stateMap.items()) {
                        int32_t stateId = std::stoi(stateIdStr);
                        int32_t shapeId = shapeIdVal.get<int32_t>();
                        stateIdToShapeId[stateId] = shapeId;
                    }
                }
            }
        }

        // Parse blocks master
        if (masterJson.contains("blocks") && masterJson["blocks"].is_array()) {
            for (auto& blockJson : masterJson["blocks"]) {
                if (!blockJson.contains("registry") || !blockJson["registry"].contains("id")) continue;

                uint16_t id = blockJson["registry"]["id"].get<uint16_t>();
                std::string identifier = blockJson["registry"].value("identifier", "");
                std::string internalName = blockJson["registry"].value("internal_name", "");
                std::string displayName = blockJson["registry"].value("display_name", "");
                int32_t minStateId = blockJson["registry"].value("min_state_id", (int32_t)id);
                int32_t maxStateId = blockJson["registry"].value("max_state_id", (int32_t)id);
                int32_t defaultStateId = blockJson["registry"].value("default_state_id", (int32_t)id);

                std::string geomClassStr = "full_cube";
                if (blockJson.contains("classification") && blockJson["classification"].contains("primary_geometry")) {
                    geomClassStr = blockJson["classification"]["primary_geometry"].get<std::string>();
                }

                GeometryClass gClass = GeometryClass::FULL_CUBE;
                if (geomClassStr == "partial_geometry") gClass = GeometryClass::PARTIAL_GEOMETRY;
                else if (geomClassStr == "multipart_structure") gClass = GeometryClass::MULTIPART_STRUCTURE;
                else if (geomClassStr == "non_solid") gClass = GeometryClass::NON_SOLID;
                else if (geomClassStr == "fluid") gClass = GeometryClass::FLUID;

                bool transparent = false;
                float hardness = 1.0f;
                int lightEmission = 0;
                if (blockJson.contains("properties")) {
                    transparent = blockJson["properties"].value("transparent", false);
                    hardness = blockJson["properties"].value("hardness", 1.0f);
                    lightEmission = blockJson["properties"].value("light_emission", 0);
                }

                bool solid = (gClass != GeometryClass::NON_SOLID && gClass != GeometryClass::FLUID);
                bool opaque = !transparent;

                BlockDefinition def;
                def.id = id;
                def.identifier = identifier;
                def.internalName = internalName;
                def.displayName = displayName;
                def.minStateId = minStateId;
                def.maxStateId = maxStateId;
                def.defaultStateId = defaultStateId;
                def.geometryClass = gClass;
                def.solid = solid;
                def.opaque = opaque;
                def.placeable = (gClass != GeometryClass::NON_SOLID);
                def.hardness = hardness;
                def.lightEmission = lightEmission;

                // Maintain custom face texture assignments for known prototypes
                if (identifier == "minecraft:grass_block" || internalName == "grass") {
                    def.texTop = 0; def.texBottom = 2; def.texSide = 3; def.texFront = 3; def.texBack = 3;
                } else if (identifier == "minecraft:dirt" || internalName == "dirt") {
                    def.texTop = 2; def.texBottom = 2; def.texSide = 2; def.texFront = 2; def.texBack = 2;
                } else if (identifier == "minecraft:stone" || internalName == "stone") {
                    def.texTop = 1; def.texBottom = 1; def.texSide = 1; def.texFront = 1; def.texBack = 1;
                } else {
                    int tex = id % 16;
                    def.texTop = tex; def.texBottom = tex; def.texSide = tex; def.texFront = tex; def.texBack = tex;
                }

                blocks[id] = def;
                if (!identifier.empty()) identifierToBlockId[identifier] = id;
                if (!internalName.empty()) identifierToBlockId[internalName] = id;

                for (int32_t sId = minStateId; sId <= maxStateId; ++sId) {
                    stateIdToBlockId[sId] = id;
                }
            }
        }
        LOGI("Registry: Catalogue loaded successfully. Registered %zu blocks and %zu state mappings.", blocks.size(), stateIdToBlockId.size());
    } catch (const std::exception& e) {
        LOGE("Failed to parse catalogue JSON: %s", e.what());
    }
}

const BlockDefinition& Registry::getBlock(uint16_t id) {
    auto it = blocks.find(id);
    if (it != blocks.end()) return it->second;
    return fallbackBlock;
}

const BlockDefinition& Registry::getBlockByIdentifier(const std::string& identifier) {
    auto it = identifierToBlockId.find(identifier);
    if (it != identifierToBlockId.end()) {
        return getBlock(it->second);
    }
    return fallbackBlock;
}

const BlockDefinition& Registry::getBlockForState(int32_t stateId) {
    auto it = stateIdToBlockId.find(stateId);
    if (it != stateIdToBlockId.end()) {
        return getBlock(it->second);
    }
    return fallbackBlock;
}

uint16_t Registry::getBlockIdForState(int32_t stateId) {
    auto it = stateIdToBlockId.find(stateId);
    if (it != stateIdToBlockId.end()) {
        return it->second;
    }
    return 0;
}

const std::vector<AABB>& Registry::getCollisionBoxesForState(int32_t stateId) {
    auto shapeIt = stateIdToShapeId.find(stateId);
    if (shapeIt != stateIdToShapeId.end()) {
        int32_t shapeId = shapeIt->second;
        auto boxesIt = shapes.find(shapeId);
        if (boxesIt != shapes.end()) {
            return boxesIt->second;
        }
    }
    // Check block solid state as fallback
    uint16_t blockId = getBlockIdForState(stateId);
    if (blockId != 0 && getBlock(blockId).solid) {
        return fullCubeBoxes;
    }
    return emptyBoxes;
}

const std::vector<AABB>& Registry::getCollisionBoxesForBlock(uint16_t blockId) {
    const BlockDefinition& def = getBlock(blockId);
    if (def.defaultStateId != 0) {
        return getCollisionBoxesForState(def.defaultStateId);
    }
    if (def.solid) return fullCubeBoxes;
    return emptyBoxes;
}

const ItemDefinition& Registry::getItem(uint16_t id) {
    auto it = items.find(id);
    if (it != items.end()) return it->second;
    return fallbackItem;
}

float Registry::getBlockTexIndex(uint16_t blockId, int faceType) {
    const BlockDefinition& def = getBlock(blockId);
    if (faceType == 2) return (float)def.texTop;
    if (faceType == 3) return (float)def.texBottom;
    return (float)def.texSide;
}

int Registry::getItemSprite(uint16_t itemId) {
    auto it = items.find(itemId);
    if (it != items.end()) return it->second.inventorySprite;
    return 0;
}

const std::unordered_map<uint16_t, BlockDefinition>& Registry::getAllBlocks() {
    return blocks;
}

