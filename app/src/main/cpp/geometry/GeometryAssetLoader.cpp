#include "GeometryAssetLoader.h"

#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <set>

namespace fincraft::geometry {
namespace {

constexpr char kMagic[] = {'F', 'C', 'G', 'B'};
constexpr std::uint16_t kBinaryVersion = 1;
constexpr std::uint16_t kHeaderBytes = 12;
constexpr std::uint32_t kVertexStride = 24;
constexpr std::uint32_t kIndexStride = 4;

std::uint16_t readU16(const std::vector<char>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset])) |
           (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8);
}

std::uint32_t readU32(const std::vector<char>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24);
}

float readF32(const std::vector<char>& bytes, std::size_t offset) {
    const std::uint32_t bits = readU32(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool rangeFits(std::uint64_t offset, std::uint64_t length, std::size_t size) {
    return offset <= size && length <= static_cast<std::uint64_t>(size) - offset;
}

bool requireFinite(float value) {
    return std::isfinite(value);
}

}  // namespace

bool loadGeometryAsset(std::istream& metadataStream,
                      std::istream& geometryStream,
                      BlockGeometryAsset& output,
                      std::string& error) {
    try {
        const nlohmann::json metadata = nlohmann::json::parse(metadataStream);
        if (metadata.at("schema_version").get<int>() != 1 ||
            metadata.at("format").get<std::string>() != "finecraft-hybrid-geometry") {
            error = "unsupported geometry metadata schema";
            return false;
        }
        if (metadata.at("coordinate_system").at("space") != "prototype_local_minecraft" ||
            metadata.at("coordinate_system").at("layout_offsets_removed") != true) {
            error = "unsupported coordinate space or laboratory offsets are present";
            return false;
        }

        std::vector<char> bytes((std::istreambuf_iterator<char>(geometryStream)), std::istreambuf_iterator<char>());
        if (bytes.size() < kHeaderBytes) {
            error = "geometry binary is shorter than its header";
            return false;
        }
        if (std::memcmp(bytes.data(), kMagic, sizeof(kMagic)) != 0) {
            error = "geometry binary magic does not match FCGB";
            return false;
        }
        if (readU16(bytes, 4) != kBinaryVersion || readU16(bytes, 6) != kHeaderBytes) {
            error = "unsupported geometry binary version or header size";
            return false;
        }
        const std::uint32_t componentCount = readU32(bytes, 8);
        const auto& geometry = metadata.at("geometry");
        const auto& components = geometry.at("components");
        if (componentCount != components.size() ||
            componentCount != geometry.at("header").at("component_count").get<std::uint32_t>()) {
            error = "binary and metadata component counts differ";
            return false;
        }
        if (geometry.at("vertex_layout").at("stride_bytes").get<std::uint32_t>() != kVertexStride ||
            geometry.at("index_layout").at("element_bytes").get<std::uint32_t>() != kIndexStride) {
            error = "unsupported vertex or index layout";
            return false;
        }

        BlockGeometryAsset parsed;
        parsed.blockId = metadata.at("block_id").get<std::string>();
        parsed.minecraftVersion = metadata.at("minecraft_version").get<std::string>();
        parsed.sourceCommit = metadata.at("source_commit").get<std::string>();
        parsed.prototypeId = metadata.at("prototype_id").get<std::int32_t>();
        parsed.states = metadata.at("states");
        if (parsed.blockId.empty() || parsed.minecraftVersion.empty() || parsed.sourceCommit.empty() || parsed.states.empty()) {
            error = "required block metadata is empty";
            return false;
        }
        std::set<std::int32_t> stateIds;
        for (const auto& state : parsed.states) {
            const auto stateId = state.at("state_id").get<std::int32_t>();
            if (stateId < 0 || !stateIds.insert(stateId).second || !state.at("state_properties").is_object()) {
                error = "state metadata is invalid or duplicated";
                return false;
            }
        }

        std::set<std::string> componentNames;
        for (const auto& componentJson : components) {
            MeshComponent component;
            component.name = componentJson.at("name").get<std::string>();
            component.stateId = componentJson.at("state_id").get<std::int32_t>();
            component.material = componentJson.value("material", nlohmann::json(nullptr));
            if (component.name.empty() || stateIds.find(component.stateId) == stateIds.end() ||
                !componentNames.insert(component.name + "#" + std::to_string(component.stateId)).second) {
                error = "component name/state identifier is empty or duplicated";
                return false;
            }
            const std::uint32_t vertexCount = componentJson.at("vertex_count").get<std::uint32_t>();
            const std::uint32_t indexCount = componentJson.at("index_count").get<std::uint32_t>();
            if (indexCount == 0 || indexCount % 3 != 0 || vertexCount == 0) {
                error = "component has no triangles or malformed counts";
                return false;
            }
            const auto& vertexBuffer = componentJson.at("vertex_buffer");
            const auto& indexBuffer = componentJson.at("index_buffer");
            const std::uint64_t vertexOffset = vertexBuffer.at("byte_offset").get<std::uint64_t>();
            const std::uint64_t vertexLength = vertexBuffer.at("byte_length").get<std::uint64_t>();
            const std::uint64_t indexOffset = indexBuffer.at("byte_offset").get<std::uint64_t>();
            const std::uint64_t indexLength = indexBuffer.at("byte_length").get<std::uint64_t>();
            if (vertexLength != static_cast<std::uint64_t>(vertexCount) * kVertexStride ||
                indexLength != static_cast<std::uint64_t>(indexCount) * kIndexStride ||
                !rangeFits(vertexOffset, vertexLength, bytes.size()) ||
                !rangeFits(indexOffset, indexLength, bytes.size())) {
                error = "component buffer range is invalid or outside the binary file";
                return false;
            }

            component.vertices.resize(vertexCount);
            for (std::uint32_t i = 0; i < vertexCount; ++i) {
                const std::size_t base = static_cast<std::size_t>(vertexOffset) + static_cast<std::size_t>(i) * kVertexStride;
                for (int axis = 0; axis < 3; ++axis) {
                    component.vertices[i].position[axis] = readF32(bytes, base + axis * sizeof(float));
                    component.vertices[i].normal[axis] = readF32(bytes, base + (axis + 3) * sizeof(float));
                    if (!requireFinite(component.vertices[i].position[axis]) || !requireFinite(component.vertices[i].normal[axis])) {
                        error = "component contains a non-finite vertex or normal";
                        return false;
                    }
                }
                const auto& n = component.vertices[i].normal;
                const float magnitude = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
                if (std::abs(magnitude - 1.0f) > 1e-4f) {
                    error = "component contains a non-normalized normal";
                    return false;
                }
            }
            component.indices.resize(indexCount);
            for (std::uint32_t i = 0; i < indexCount; ++i) {
                const std::uint32_t index = readU32(bytes, static_cast<std::size_t>(indexOffset) + static_cast<std::size_t>(i) * kIndexStride);
                if (index >= vertexCount) {
                    error = "component index is outside its vertex buffer";
                    return false;
                }
                component.indices[i] = index;
            }
            parsed.components.push_back(std::move(component));
        }

        for (const auto& state : parsed.states) {
            const int stateId = state.at("state_id").get<int>();
            const int shapeId = state.at("source_shape_id").get<int>();
            for (const auto& box : state.at("collision_boxes")) {
                if (box.size() != 6) {
                    error = "collision box must contain six bounds";
                    return false;
                }
                CollisionBox parsedBox;
                parsedBox.stateId = stateId;
                parsedBox.sourceShapeId = shapeId;
                for (int axis = 0; axis < 3; ++axis) {
                    parsedBox.minimum[axis] = box.at(axis).get<float>();
                    parsedBox.maximum[axis] = box.at(axis + 3).get<float>();
                    if (!requireFinite(parsedBox.minimum[axis]) || !requireFinite(parsedBox.maximum[axis]) ||
                        parsedBox.minimum[axis] > parsedBox.maximum[axis]) {
                        error = "collision box contains invalid bounds";
                        return false;
                    }
                }
                parsed.collisionBoxes.push_back(parsedBox);
            }
        }
        output = std::move(parsed);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

}  // namespace fincraft::geometry
