#include "Registry.h"
#include <stdexcept>
#include <android/asset_manager.h>
#include <android/log.h>
#include <nlohmann/json.hpp>

#define LOG_TAG "Registry"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using json = nlohmann::json;

std::unordered_map<uint16_t, BlockDefinition> Registry::blocks;
std::unordered_map<uint16_t, ItemDefinition> Registry::items;
extern AAssetManager* globalAssetManager;

std::string readAsset(const char* path) {
    if (!globalAssetManager) return "";
    AAsset* asset = AAssetManager_open(globalAssetManager, path, AASSET_MODE_BUFFER);
    if (!asset) return "";
    off_t length = AAsset_getLength(asset);
    std::string result(length, '\0');
    AAsset_read(asset, &result[0], length);
    AAsset_close(asset);
    return result;
}

void Registry::registerBlock(uint16_t id, std::string identifier, std::string displayName, int texAll, bool opaque, bool solid) {
    blocks[id] = {id, identifier, displayName, true, solid, opaque, 64, 1.0f, texAll, texAll, texAll, texAll, texAll};
}

void Registry::registerBlock(uint16_t id, std::string identifier, std::string displayName, int texTop, int texBottom, int texSide, bool opaque, bool solid) {
    blocks[id] = {id, identifier, displayName, true, solid, opaque, 64, 1.0f, texTop, texBottom, texSide, texSide, texSide};
}

void Registry::registerItem(uint16_t id, std::string identifier, std::string displayName, int inventorySprite, uint16_t maxStack, uint16_t blockId) {
    items[id] = {id, identifier, displayName, maxStack, blockId, inventorySprite};
}

void Registry::init() {
    blocks.clear();
    items.clear();
    blocks[0] = {0, "air", "Air", false, false, false, 0, 0.0f, 0, 0, 0, 0, 0};

    std::unordered_map<std::string, int> atlasMap;
    std::string atlasStr = readAsset("atlas_map.json");
    if (!atlasStr.empty()) {
        try {
            auto j = json::parse(atlasStr);
            for (auto& el : j.items()) {
                atlasMap[el.key()] = el.value().get<int>();
            }
        } catch (...) {
            LOGE("Failed to parse atlas_map.json");
        }
    }

    auto getTex = [&](const std::string& name) {
        if (atlasMap.find(name) != atlasMap.end()) return atlasMap[name];
        return 4095;
    };

    std::unordered_map<std::string, uint16_t> preset_ids = {
        {"grass_block", 1},
        {"dirt", 2},
        {"stone", 3},
        {"oak_log", 4},
        {"oak_leaves", 5},
        {"water", 6},
        {"oak_planks", 7},
        {"sand", 8},
        {"glass", 9},
        {"bricks", 10},
        {"gravel", 11},
        {"coal_ore", 12},
        {"iron_ore", 13},
        {"diamond_ore", 14},
        {"cobblestone", 15}
    };

    std::string blocksStr = readAsset("blocks.json");
    if (!blocksStr.empty()) {
        try {
            auto j = json::parse(blocksStr);
            uint16_t fc_id = 16;
            for (auto& el : j.items()) {
                std::string mc_id = el.key();
                if (mc_id.find("minecraft:") != 0) continue;
                std::string identifier = mc_id.substr(10);
                
                uint16_t block_id = fc_id;
                if (preset_ids.find(identifier) != preset_ids.end()) {
                    block_id = preset_ids[identifier];
                } else {
                    fc_id++;
                }
                
                int texTop = getTex(identifier + "_top");
                int texBottom = getTex(identifier + "_bottom");
                int texSide = getTex(identifier + "_side");
                int texAll = getTex(identifier);
                
                // Specific overrides for missing faces in Faithful
                if (identifier == "grass_block" && texBottom == 4095) {
                    texBottom = getTex("dirt");
                }
                if (identifier == "oak_log" && texTop != 4095) {
                    texBottom = texTop;
                }
                if (identifier == "water") {
                    texAll = getTex("water_still");
                    texTop = texBottom = texSide = texAll;
                }
                if (identifier == "glass") {
                    // Glass is fine, uses glass.png
                }
                
                bool opaque = true;
                bool solid = true;
                
                if (identifier == "oak_leaves" || identifier == "glass") opaque = false;
                if (identifier == "water") { opaque = false; solid = false; }
                
                if (identifier == "water") {
                    texAll = getTex("water_still");
                }
                
                int top = texTop != 4095 ? texTop : (texAll != 4095 ? texAll : texSide);
                int bottom = texBottom != 4095 ? texBottom : (texAll != 4095 ? texAll : texSide);
                int side = texSide != 4095 ? texSide : (texAll != 4095 ? texAll : top);
                
                if (top == 4095 && bottom == 4095 && side == 4095) {
                    LOGI("MISSING TEXTURE FOR BLOCK: %s", identifier.c_str());
                    top = bottom = side = 4095;
                }
                
                registerBlock(block_id, identifier, identifier, top, bottom, side, opaque, solid);
                
                int invSprite = texAll != 4095 ? texAll : (texSide != 4095 ? texSide : texTop);
                registerItem(block_id, identifier, identifier, invSprite, 64, block_id);
            }
            LOGI("Loaded %d blocks from JSON", blocks.size());
        } catch (...) {
            LOGE("Failed to parse blocks.json");
        }
    } else {
        LOGE("Failed to read blocks.json");
    }

    std::string itemsStr = readAsset("items.json");
    if (!itemsStr.empty()) {
        try {
            auto j = json::parse(itemsStr);
            uint16_t item_id = 256; // Start item IDs after blocks
            
            std::unordered_map<std::string, uint16_t> preset_item_ids = {
                {"apple", 100},
                {"diamond_sword", 101},
                {"diamond_pickaxe", 102},
                {"wooden_pickaxe", 103},
                {"stick", 104}
            };
            
            for (auto& el : j) {
                std::string identifier = "";
                if (el.contains("components") && el["components"].contains("minecraft:item_model")) {
                    identifier = el["components"]["minecraft:item_model"].get<std::string>();
                    if (identifier.find("minecraft:") == 0) identifier = identifier.substr(10);
                }
                if (identifier.empty()) continue;
                
                uint16_t current_id = item_id;
                if (preset_item_ids.find(identifier) != preset_item_ids.end()) {
                    current_id = preset_item_ids[identifier];
                } else {
                    item_id++;
                }

                
                std::string displayName = identifier;
                for (int i = 0; i < displayName.length(); i++) {
                    if (displayName[i] == '_') displayName[i] = ' ';
                }
                
                int inventorySprite = getTex(identifier);
                if (inventorySprite == 4095) {
                    LOGI("MISSING TEXTURE FOR ITEM: %s", identifier.c_str());
                }
                
                registerItem(current_id, identifier, displayName, inventorySprite);
            }
            LOGI("Loaded %d items from JSON", items.size());
        } catch (...) {
            LOGE("Failed to parse items.json");
        }
    } else {
        LOGE("Failed to read items.json");
    }
}


const BlockDefinition& Registry::getBlock(uint16_t id) {
    auto it = blocks.find(id);
    if (it != blocks.end()) return it->second;
    static BlockDefinition fallback = {0, "unknown", "Unknown", false, false, false, 0, 0.0f, 0, 0, 0, 0, 0};
    return fallback;
}

const ItemDefinition& Registry::getItem(uint16_t id) {
    auto it = items.find(id);
    if (it != items.end()) return it->second;
    static ItemDefinition fallback = {0, "unknown", "Unknown", 0, 0, 0};
    return fallback;
}

float Registry::getBlockTexIndex(uint16_t blockId, int faceType) {
    const auto& block = getBlock(blockId);
    int tex = block.texSide;
    if (faceType == 2) tex = block.texTop;
    else if (faceType == 3) tex = block.texBottom;
    else if (faceType == 0) tex = block.texFront;
    else if (faceType == 1) tex = block.texBack;

    float tint = 0.0f;
    if (block.identifier == "water") {
        tint = 0.2f;
    } else if (block.identifier.find("leaves") != std::string::npos || 
               block.identifier.find("grass") != std::string::npos || 
               block.identifier.find("fern") != std::string::npos || 
               block.identifier.find("vine") != std::string::npos) {
        // Grass block side shouldn't be tinted, but leaves and grass top should
        if (block.identifier != "grass_block" || faceType == 2) {
            tint = 0.1f;
        }
    }
    return (float)tex + tint;
}

int Registry::getItemSprite(uint16_t itemId) {
    return getItem(itemId).inventorySprite;
}

const std::unordered_map<uint16_t, BlockDefinition>& Registry::getAllBlocks() {
    return blocks;
}
