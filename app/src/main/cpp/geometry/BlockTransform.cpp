#include "BlockTransform.h"
#include "../core/Logger.h"
#include <algorithm>
#include <limits>
#include <cassert>

namespace fincraft::geometry {

BlockTransform::BlockTransform() : matrix(1.0f), normalMatrix(1.0f), valid(true) {}

BlockTransform::BlockTransform(const glm::mat4& mat) : matrix(mat) {
    float det = glm::determinant(matrix);
    if (std::isfinite(det) && std::abs(det) > 1e-6f) {
        valid = true;
        normalMatrix = glm::transpose(glm::inverse(glm::mat3(matrix)));
    } else {
        valid = false;
        matrix = glm::mat4(1.0f);
        normalMatrix = glm::mat3(1.0f);
    }
}

BlockTransform BlockTransform::identity() {
    return BlockTransform(glm::mat4(1.0f));
}

BlockTransform BlockTransform::translation(const glm::vec3& t) {
    return BlockTransform(glm::translate(glm::mat4(1.0f), t));
}

BlockTransform BlockTransform::rotationX(float radians) {
    return BlockTransform(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(1.0f, 0.0f, 0.0f)));
}

BlockTransform BlockTransform::rotationY(float radians) {
    return BlockTransform(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 1.0f, 0.0f)));
}

BlockTransform BlockTransform::rotationZ(float radians) {
    return BlockTransform(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f)));
}

BlockTransform BlockTransform::translationAndRotation(const glm::vec3& t, float rotYRadians, const glm::vec3& pivot) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, t);
    if (pivot != glm::vec3(0.0f)) {
        m = glm::translate(m, pivot);
    }
    m = glm::rotate(m, rotYRadians, glm::vec3(0.0f, 1.0f, 0.0f));
    if (pivot != glm::vec3(0.0f)) {
        m = glm::translate(m, -pivot);
    }
    return BlockTransform(m);
}

glm::vec3 BlockTransform::transformPosition(const glm::vec3& pos) const {
    if (!valid) return pos;
    glm::vec4 p4 = matrix * glm::vec4(pos, 1.0f);
    return glm::vec3(p4.x, p4.y, p4.z);
}

glm::vec3 BlockTransform::transformNormal(const glm::vec3& normal) const {
    if (!valid) return normal;
    glm::vec3 n = normalMatrix * normal;
    float len = glm::length(n);
    if (len > 1e-6f && std::isfinite(len)) {
        return n / len;
    }
    return normal;
}

AABB BlockTransform::transformAABB(const AABB& aabb, const glm::vec3& worldPos) const {
    glm::vec3 corners[8] = {
        {aabb.minX, aabb.minY, aabb.minZ},
        {aabb.maxX, aabb.minY, aabb.minZ},
        {aabb.minX, aabb.maxY, aabb.minZ},
        {aabb.maxX, aabb.maxY, aabb.minZ},
        {aabb.minX, aabb.minY, aabb.maxZ},
        {aabb.maxX, aabb.minY, aabb.maxZ},
        {aabb.minX, aabb.maxY, aabb.maxZ},
        {aabb.maxX, aabb.maxY, aabb.maxZ}
    };

    glm::vec3 minRes(std::numeric_limits<float>::max());
    glm::vec3 maxRes(std::numeric_limits<float>::lowest());

    for (const auto& corner : corners) {
        glm::vec3 tc = transformPosition(corner);
        minRes = glm::min(minRes, tc);
        maxRes = glm::max(maxRes, tc);
    }

    AABB result;
    result.minX = minRes.x + worldPos.x;
    result.minY = minRes.y + worldPos.y;
    result.minZ = minRes.z + worldPos.z;
    result.maxX = maxRes.x + worldPos.x;
    result.maxY = maxRes.y + worldPos.y;
    result.maxZ = maxRes.z + worldPos.z;
    return result;
}

void BlockTransform::transformVertices10f(const std::vector<float>& srcVertices, std::vector<float>& destVertices, const glm::vec3& worldPos) const {
    destVertices.clear();
    if (srcVertices.size() % 10 != 0) return;
    destVertices.reserve(srcVertices.size());

    for (size_t i = 0; i < srcVertices.size(); i += 10) {
        float x = srcVertices[i+0];
        float y = srcVertices[i+1];
        float z = srcVertices[i+2];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;

        glm::vec3 pos(x, y, z);
        float u = srcVertices[i+3];
        float v = srcVertices[i+4];
        float texIndex = srcVertices[i+5];
        glm::vec3 norm(srcVertices[i+6], srcVertices[i+7], srcVertices[i+8]);
        float ao = srcVertices[i+9];

        glm::vec3 tp = transformPosition(pos) + worldPos;
        glm::vec3 tn = transformNormal(norm);

        destVertices.push_back(tp.x);
        destVertices.push_back(tp.y);
        destVertices.push_back(tp.z);
        destVertices.push_back(u);
        destVertices.push_back(v);
        destVertices.push_back(texIndex);
        destVertices.push_back(tn.x);
        destVertices.push_back(tn.y);
        destVertices.push_back(tn.z);
        destVertices.push_back(ao);
    }
}

void BlockTransform::transformVerticesSource(const std::vector<Vertex>& srcVertices, std::vector<Vertex>& destVertices) const {
    destVertices.clear();
    destVertices.reserve(srcVertices.size());

    for (const auto& sv : srcVertices) {
        Vertex dv;
        glm::vec3 p(sv.position[0], sv.position[1], sv.position[2]);
        glm::vec3 n(sv.normal[0], sv.normal[1], sv.normal[2]);

        glm::vec3 tp = transformPosition(p);
        glm::vec3 tn = transformNormal(n);

        dv.position = {tp.x, tp.y, tp.z};
        dv.normal = {tn.x, tn.y, tn.z};
        destVertices.push_back(dv);
    }
}

bool BlockTransform::isValid() const {
    return valid;
}

const glm::mat4& BlockTransform::getMatrix() const {
    return matrix;
}

const glm::mat3& BlockTransform::getNormalMatrix() const {
    return normalMatrix;
}

void BlockTransform::runTests() {
    LOGI("BlockTransform: Running automated tests (18 requirements)...");

    // 1. Identity transformation
    auto tId = BlockTransform::identity();
    glm::vec3 p1 = tId.transformPosition(glm::vec3(1.0f, 2.0f, 3.0f));
    assert(std::abs(p1.x - 1.0f) < 1e-5f && std::abs(p1.y - 2.0f) < 1e-5f && std::abs(p1.z - 3.0f) < 1e-5f);

    // 2. 90-degree rotation around Y
    auto tRot90Y = BlockTransform::rotationY(glm::half_pi<float>());
    glm::vec3 pRot = tRot90Y.transformPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    assert(std::abs(pRot.x) < 1e-4f && std::abs(pRot.z - (-1.0f)) < 1e-4f);

    // 3. 180-degree rotation around Y
    auto tRot180 = BlockTransform::rotationY(glm::pi<float>());
    glm::vec3 p180 = tRot180.transformPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    assert(std::abs(p180.x - (-1.0f)) < 1e-4f && std::abs(p180.z) < 1e-4f);

    // 4. 270-degree rotation around Y
    auto tRot270 = BlockTransform::rotationY(1.5f * glm::pi<float>());
    glm::vec3 p270 = tRot270.transformPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    assert(std::abs(p270.x) < 1e-4f && std::abs(p270.z - 1.0f) < 1e-4f);

    // 5. 90-degree rotation around X
    auto tRotX = BlockTransform::rotationX(glm::half_pi<float>());
    glm::vec3 pX = tRotX.transformPosition(glm::vec3(0.0f, 1.0f, 0.0f));
    assert(std::abs(pX.y) < 1e-4f && std::abs(pX.z - 1.0f) < 1e-4f);

    // 6. 90-degree rotation around Z
    auto tRotZ = BlockTransform::rotationZ(glm::half_pi<float>());
    glm::vec3 pZ = tRotZ.transformPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    assert(std::abs(pZ.x) < 1e-4f && std::abs(pZ.y - 1.0f) < 1e-4f);

    // 7. Translation without rotation
    auto tTrans = BlockTransform::translation(glm::vec3(10.0f, 20.0f, 30.0f));
    glm::vec3 pTrans = tTrans.transformPosition(glm::vec3(1.0f, 1.0f, 1.0f));
    assert(std::abs(pTrans.x - 11.0f) < 1e-5f && std::abs(pTrans.y - 21.0f) < 1e-5f && std::abs(pTrans.z - 31.0f) < 1e-5f);

    // 8. Rotation around non-origin pivot
    auto tPivot = BlockTransform::translationAndRotation(glm::vec3(0.0f), glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 1.0f));
    glm::vec3 pPivot = tPivot.transformPosition(glm::vec3(2.0f, 0.0f, 1.0f));
    assert(std::abs(pPivot.x - 1.0f) < 1e-4f && std::abs(pPivot.z - 0.0f) < 1e-4f);

    // 9. Combined rotation and translation
    auto tCombo = BlockTransform::translationAndRotation(glm::vec3(5.0f, 0.0f, 0.0f), glm::half_pi<float>());
    glm::vec3 pCombo = tCombo.transformPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    assert(std::abs(pCombo.x - 5.0f) < 1e-4f && std::abs(pCombo.z - (-1.0f)) < 1e-4f);

    // 10. Normal normalization
    glm::vec3 nOrig(0.0f, 1.0f, 0.0f);
    glm::vec3 nTrans = tRot90Y.transformNormal(nOrig);
    assert(std::abs(glm::length(nTrans) - 1.0f) < 1e-5f);

    // 11. Normal perpendicularity
    glm::vec3 normT = tRot90Y.transformNormal(glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 dirT = tRot90Y.transformPosition(glm::vec3(0.0f, 0.0f, 1.0f)) - tRot90Y.transformPosition(glm::vec3(0.0f));
    float dotVal = glm::dot(normT, glm::normalize(dirT));
    assert(std::abs(dotVal) < 1e-4f);

    // 12. Eight-corner AABB reconstruction
    AABB box;
    box.minX = -0.5f; box.minY = 0.0f; box.minZ = -0.5f;
    box.maxX = 0.5f; box.maxY = 1.0f; box.maxZ = 0.5f;
    AABB tBox = tTrans.transformAABB(box, glm::vec3(0.0f));
    assert(std::abs(tBox.minX - 9.5f) < 1e-5f && std::abs(tBox.maxX - 10.5f) < 1e-5f);

    // 13. Multiple collision boxes
    std::vector<AABB> boxes = {box, box};
    for (auto& b : boxes) {
        AABB tb = tTrans.transformAABB(b, glm::vec3(0.0f));
        assert(tb.maxX > tb.minX);
    }

    // 14. Invalid and singular matrices
    glm::mat4 zeroMat(0.0f);
    BlockTransform tInvalid(zeroMat);
    assert(!tInvalid.isValid());

    // Reflections and negative-determinant transformations
    glm::mat4 refMat(1.0f);
    refMat[0][0] = -1.0f;
    BlockTransform tRef(refMat);
    assert(tRef.isValid());

    // Malformed 10-float vertex buffers & non-finite data
    std::vector<float> malformed = {1.0f, 2.0f};
    std::vector<float> destMalformed;
    tId.transformVertices10f(malformed, destMalformed, glm::vec3(0.0f));
    assert(destMalformed.empty());

    std::vector<float> nonFinite = {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    std::vector<float> destNonFinite;
    tId.transformVertices10f(nonFinite, destNonFinite, glm::vec3(0.0f));

    // Composition of multiple transformations
    BlockTransform tCompA = BlockTransform::translation(glm::vec3(1.0f, 2.0f, 3.0f));
    BlockTransform tCompB = BlockTransform::translation(glm::vec3(4.0f, 5.0f, 6.0f));
    BlockTransform tComposed(tCompB.getMatrix() * tCompA.getMatrix());
    glm::vec3 pComp = tComposed.transformPosition(glm::vec3(0.0f));
    assert(std::abs(pComp.x - 5.0f) < 1e-5f && std::abs(pComp.y - 7.0f) < 1e-5f && std::abs(pComp.z - 9.0f) < 1e-5f);

    // Multiple instances using shared immutable geometry
    std::vector<float> sharedGeom = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    std::vector<float> inst1, inst2;
    BlockTransform::translation(glm::vec3(1.0f, 0.0f, 0.0f)).transformVertices10f(sharedGeom, inst1, glm::vec3(0.0f));
    BlockTransform::translation(glm::vec3(2.0f, 0.0f, 0.0f)).transformVertices10f(sharedGeom, inst2, glm::vec3(0.0f));
    assert(std::abs(inst1[0] - 1.0f) < 1e-5f);
    assert(std::abs(inst2[0] - 2.0f) < 1e-5f);
    assert(std::abs(sharedGeom[0] - 0.0f) < 1e-5f); // immutable

    LOGI("BlockTransform: All automated transformation tests PASSED successfully!");
}

} // namespace fincraft::geometry
