#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
    uint16_t blockId; // 0 if it doesn't place a block
    int inventorySprite; // Atlas tile index
};

class Registry {
public:
    static void init();
    static const BlockDefinition& getBlock(uint16_t id);
    static const ItemDefinition& getItem(uint16_t id);
    static const std::unordered_map<uint16_t, BlockDefinition>& getAllBlocks();
    
    static float getBlockTexIndex(uint16_t blockId, int faceType); // faceType: 0=Front, 1=Back, 2=Top, 3=Bottom, 4=Right, 5=Left
    static int getItemSprite(uint16_t itemId);

private:
    static std::unordered_map<uint16_t, BlockDefinition> blocks;
    static std::unordered_map<uint16_t, ItemDefinition> items;
    
    static void registerBlock(uint16_t id, std::string identifier, std::string displayName, int texAll, bool opaque = true, bool solid = true);
    static void registerBlock(uint16_t id, std::string identifier, std::string displayName, int texTop, int texBottom, int texSide, bool opaque = true, bool solid = true);
    
    static void registerItem(uint16_t id, std::string identifier, std::string displayName, int inventorySprite, uint16_t maxStack = 64, uint16_t blockId = 0);
};
