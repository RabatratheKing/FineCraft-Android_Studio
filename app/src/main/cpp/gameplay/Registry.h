#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <istream>
#include <android/asset_manager.h>
#include "../core/Globals.h"

struct AABB {
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 1.0f, maxY = 1.0f, maxZ = 1.0f;
};

enum class GeometryClass {
    FULL_CUBE,
    PARTIAL_GEOMETRY,
    MULTIPART_STRUCTURE,
    NON_SOLID,
    FLUID,
    UNKNOWN
};

struct BlockDefinition {
    uint16_t id = 0;
    std::string identifier;        // e.g. "minecraft:stone"
    std::string internalName;      // e.g. "stone"
    std::string displayName;       // e.g. "Stone"
    int32_t minStateId = 0;
    int32_t maxStateId = 0;
    int32_t defaultStateId = 0;

    GeometryClass geometryClass = GeometryClass::FULL_CUBE;
    bool solid = true;
    bool opaque = true;
    bool placeable = true;
    float hardness = 1.0f;
    int lightEmission = 0;

    int texTop = 0;
    int texBottom = 0;
    int texSide = 0;
    int texFront = 0;
    int texBack = 0;
};

struct ItemDefinition {
    uint16_t id = 0;
    std::string identifier;
    std::string displayName;
    uint16_t maxStack = 64;
    uint16_t blockId = 0;
    int inventorySprite = 0;
};

class Registry {
public:
    static void init();
    static void loadCatalogueFromAssets(AAssetManager* assetManager);
    static void loadCatalogueFromStreams(std::istream& blocksMasterStream, std::istream& collisionShapesStream);

    static const BlockDefinition& getBlock(uint16_t id);
    static const BlockDefinition& getBlockByIdentifier(const std::string& identifier);
    static const BlockDefinition& getBlockForState(int32_t stateId);
    static uint16_t getBlockIdForState(int32_t stateId);

    static const std::vector<AABB>& getCollisionBoxesForState(int32_t stateId);
    static const std::vector<AABB>& getCollisionBoxesForBlock(uint16_t blockId);

    static const ItemDefinition& getItem(uint16_t id);
    static const std::unordered_map<uint16_t, BlockDefinition>& getAllBlocks();

    static float getBlockTexIndex(uint16_t blockId, int faceType);
    static int getItemSprite(uint16_t itemId);
    static StateId getDefaultStateIdForLegacyId(uint8_t legacyId);

private:
    static std::unordered_map<uint16_t, BlockDefinition> blocks;
    static std::unordered_map<std::string, uint16_t> identifierToBlockId;
    static std::unordered_map<int32_t, uint16_t> stateIdToBlockId;
    static std::unordered_map<int32_t, int32_t> stateIdToShapeId;
    static std::unordered_map<int32_t, std::vector<AABB>> shapes;
    static std::unordered_map<uint16_t, ItemDefinition> items;
    static bool initialized;

    static const BlockDefinition fallbackBlock;
    static const ItemDefinition fallbackItem;
    static const std::vector<AABB> fullCubeBoxes;
    static const std::vector<AABB> emptyBoxes;
};

