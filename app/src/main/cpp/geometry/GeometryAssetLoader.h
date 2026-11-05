#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <string>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include "../gameplay/Registry.h"

namespace fincraft::geometry {

struct Vertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
};

struct MeshComponent {
    std::string name;
    std::int32_t stateId = 0;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    nlohmann::json material = nullptr;
};

struct CollisionBox {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
    std::int32_t stateId = 0;
    std::int32_t sourceShapeId = 0;
};

struct BlockGeometryAsset {
    std::string blockId;
    std::string minecraftVersion;
    std::string sourceCommit;
    std::int32_t prototypeId = 0;
    nlohmann::json states = nlohmann::json::array();
    std::vector<MeshComponent> components;
    std::vector<CollisionBox> collisionBoxes;
};

// Stream-based so Android callers can pass AAsset streams without extracting
// the packaged geometry files to disk. No OpenGL calls are made here.
bool loadGeometryAsset(
    std::istream& metadataStream,
    std::istream& geometryStream,
    BlockGeometryAsset& output,
    std::string& error);

struct ComponentMesh10f {
    std::string name;
    std::int32_t stateId = 0;
    std::vector<float> vertices10f; // 10 floats per vertex: x, y, z, u, v, texIndex, nx, ny, nz, ao
    std::vector<std::uint32_t> indices;
    std::vector<AABB> collisionBoxes;
};

struct LoadedPrototype {
    std::string blockId;
    std::uint16_t numericBlockId = 0;
    BlockGeometryAsset asset;
    std::vector<ComponentMesh10f> components;
};

class GeometryManager {
public:
    static void init();
    static void loadPrototypesFromAssets(void* assetManager);
    static const LoadedPrototype* getPrototype(std::uint16_t blockId);
    static const LoadedPrototype* getPrototypeForState(std::int32_t stateId);
    static bool hasPrototype(std::uint16_t blockId);
    static bool hasPrototypeForState(std::int32_t stateId);

private:
    static std::unordered_map<std::uint16_t, LoadedPrototype> prototypesByBlockId;
    static std::unordered_map<std::int32_t, std::uint16_t> stateToPrototypeBlockId;
};

}  // namespace fincraft::geometry
