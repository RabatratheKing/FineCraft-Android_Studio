#pragma once
#include "../core/Globals.h"
#include <glm/glm.hpp>

struct RaycastHit {
    bool hasHit;
    int bx, by, bz;
    int px, py, pz; // adjacent block
    glm::vec3 normal;
};

void tickInteraction(float dt);
RaycastHit getTargetBlock();
float getBlockHardness(uint8_t blockId);
