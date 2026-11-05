#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include "GeometryAssetLoader.h"

namespace fincraft::geometry {

class BlockTransform {
public:
    BlockTransform();
    explicit BlockTransform(const glm::mat4& matrix);

    static BlockTransform identity();
    static BlockTransform translation(const glm::vec3& t);
    static BlockTransform rotationX(float radians);
    static BlockTransform rotationY(float radians);
    static BlockTransform rotationZ(float radians);
    static BlockTransform translationAndRotation(const glm::vec3& t, float rotYRadians, const glm::vec3& pivot = glm::vec3(0.0f));

    glm::vec3 transformPosition(const glm::vec3& pos) const;
    glm::vec3 transformNormal(const glm::vec3& normal) const;

    AABB transformAABB(const AABB& aabb, const glm::vec3& worldPos) const;

    void transformVertices10f(const std::vector<float>& srcVertices, std::vector<float>& destVertices, const glm::vec3& worldPos) const;
    void transformVerticesSource(const std::vector<Vertex>& srcVertices, std::vector<Vertex>& destVertices) const;

    bool isValid() const;
    const glm::mat4& getMatrix() const;
    const glm::mat3& getNormalMatrix() const;

    static void runTests();

private:
    glm::mat4 matrix{1.0f};
    glm::mat3 normalMatrix{1.0f};
    bool valid = true;
};

} // namespace fincraft::geometry
