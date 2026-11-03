#include "Registry.h"
#include <unordered_map>

std::unordered_map<uint16_t, BlockDefinition> Registry::blocks;
std::unordered_map<uint16_t, ItemDefinition> Registry::items;

void Registry::init() {
    blocks.clear();
    items.clear();
    // Essential AIR/empty sentinel (ID 0)
    blocks[0] = {0, "air", "Air", false, false, false, 0, 0.0f, 0, 0, 0, 0, 0};
}

const BlockDefinition& Registry::getBlock(uint16_t id) {
    auto it = blocks.find(id);
    if (it != blocks.end()) return it->second;
    static BlockDefinition fallback = {0, "air", "Air", false, false, false, 0, 0.0f, 0, 0, 0, 0, 0};
    return fallback;
}

const ItemDefinition& Registry::getItem(uint16_t id) {
    auto it = items.find(id);
    if (it != items.end()) return it->second;
    static ItemDefinition fallback = {0, "air", "Air", 0, 0, 0};
    return fallback;
}

float Registry::getBlockTexIndex(uint16_t blockId, int faceType) {
    return 0.0f;
}

int Registry::getItemSprite(uint16_t itemId) {
    return 0;
}

const std::unordered_map<uint16_t, BlockDefinition>& Registry::getAllBlocks() {
    return blocks;
}
