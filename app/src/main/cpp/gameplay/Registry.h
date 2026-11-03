#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

struct BlockDefinition {
    uint16_t id;
    std::string identifier;
    std::string displayName;
    bool placeable;
    bool solid;
    bool opaque;
    uint16_t maxStack;
    float hardness;

    int texTop;
    int texBottom;
    int texSide;
    int texFront;
    int texBack;
};

struct ItemDefinition {
    uint16_t id;
    std::string identifier;
    std::string displayName;
    uint16_t maxStack;
    uint16_t blockId;
    int inventorySprite;
};

class Registry {
public:
    static void init();
    static const BlockDefinition& getBlock(uint16_t id);
    static const ItemDefinition& getItem(uint16_t id);
    static const std::unordered_map<uint16_t, BlockDefinition>& getAllBlocks();
    
    static float getBlockTexIndex(uint16_t blockId, int faceType);
    static int getItemSprite(uint16_t itemId);

private:
    static std::unordered_map<uint16_t, BlockDefinition> blocks;
    static std::unordered_map<uint16_t, ItemDefinition> items;
};
