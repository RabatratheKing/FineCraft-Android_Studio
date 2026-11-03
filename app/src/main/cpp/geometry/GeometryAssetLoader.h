#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

}  // namespace fincraft::geometry
