#include "Raycast.h"
#include "Registry.h"
#include "../core/Globals.h"
#include "Inventory.h"
#include "../world/World.h"
#include "../player/Physics.h"

float getBlockHardness(uint8_t blockId) {
    switch (blockId) {
        case 1: return 0.5f; // Grass
        case 2: return 0.5f; // Dirt
        case 3: return 1.5f; // Stone
        case 4: return 1.0f; // Wood
        case 5: return 0.2f; // Leaves
        case 7: return 1.0f; // Planks
        case 8: return 0.3f; // Sand
        case 10: return 0.5f; // Scaffolding
        case 6: return -1.0f; // Water (unbreakable)
        default: return 1.0f;
    }
}

RaycastHit getTargetBlock() {
    float eyeY = cameraEyeY.load();
    glm::vec3 pos = glm::vec3(playerX.load(), playerY.load() + eyeY, playerZ.load());
    
    float cLookX = cameraLookX.load();
    float cLookY = cameraLookY.load();
    glm::vec3 front;
    front.x = cos(glm::radians(cLookX)) * cos(glm::radians(cLookY));
    front.y = sin(glm::radians(cLookY));
    front.z = sin(glm::radians(cLookX)) * cos(glm::radians(cLookY));
    front = glm::normalize(front);
    
    glm::vec3 prevBlock(-1.0f);
    
    for (float i = 0; i < INTERACTION_REACH; i += 0.05f) {
        glm::vec3 rayPos = pos + front * i;
        int bx = (int)floor(rayPos.x);
        int by = (int)floor(rayPos.y);
        int bz = (int)floor(rayPos.z);
        
        int cx = bx / CHUNK_SIZE;
        int cz = bz / CHUNK_SIZE;
        if (bx < 0 && bx % CHUNK_SIZE != 0) cx -= 1;
        if (bz < 0 && bz % CHUNK_SIZE != 0) cz -= 1;
        
        int lx = (bx % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (bz % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        
        if (by >= 0 && by < CHUNK_HEIGHT) {
            std::lock_guard<std::mutex> lock(worldMutex);
            if (chunks.find({cx, cz}) != chunks.end() && chunks[{cx, cz}].state >= ChunkState::Generated) {
                uint8_t currentBlock = chunks[{cx, cz}].data[lx][by][lz];
                if (currentBlock != 0 && currentBlock != 6) { // solid block (not air, not water)
                    int px = bx, py = by, pz = bz;
                    glm::vec3 normal(0.0f);
                    if (prevBlock.x != -1.0f) {
                        px = (int)floor(prevBlock.x);
                        py = (int)floor(prevBlock.y);
                        pz = (int)floor(prevBlock.z);
                        normal = glm::vec3(px - bx, py - by, pz - bz);
                    }
                    return {true, bx, by, bz, px, py, pz, normal};
                }
            }
        }
        prevBlock = rayPos;
    }
    return {false, 0, 0, 0, 0, 0, 0, glm::vec3(0)};
}

void tickInteraction(float dt) {
    if (isDead.load()) return;
    float pCooldown = placeCooldown.load();
    if (pCooldown > 0) placeCooldown.store(pCooldown - dt);
    float bCooldown = breakCooldown.load();
    if (bCooldown > 0) breakCooldown.store(bCooldown - dt);

    RaycastHit hit = getTargetBlock();

    if (!hit.hasHit) {
        miningProgress.store(0.0f);
        return;
    }

    // Check if target changed
    int mTargetX = miningTargetX.load();
    int mTargetY = miningTargetY.load();
    int mTargetZ = miningTargetZ.load();
    if (hit.bx != mTargetX || hit.by != mTargetY || hit.bz != mTargetZ) {
        miningProgress.store(0.0f);
        miningTargetX.store(hit.bx);
        miningTargetY.store(hit.by);
        miningTargetZ.store(hit.bz);
    }

    if (inputBreak.load()) {
        if (breakCooldown.load() <= 0.0f) {
            uint8_t targetBlockId = 0;
            int cx = hit.bx / CHUNK_SIZE;
            int cz = hit.bz / CHUNK_SIZE;
            if (hit.bx < 0 && hit.bx % CHUNK_SIZE != 0) cx -= 1;
            if (hit.bz < 0 && hit.bz % CHUNK_SIZE != 0) cz -= 1;
            int lx = (hit.bx % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
            int lz = (hit.bz % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
            
            {
                std::lock_guard<std::mutex> lock(worldMutex);
                if (chunks.find({cx, cz}) != chunks.end()) {
                    targetBlockId = chunks[{cx, cz}].data[lx][hit.by][lz];
                }
            }

            float hardness = getBlockHardness(targetBlockId);
            if (hardness > 0.0f) {
                float mProg = miningProgress.load() + (dt / hardness);
                miningProgress.store(mProg);
                if (mProg >= 1.0f) {
                    miningProgress.store(0.0f);
                    breakCooldown.store(0.2f);
                    
                    if (addItem(targetBlockId, 1)) {
                        std::lock_guard<std::mutex> lock(worldMutex);
                        chunks[{cx, cz}].data[lx][hit.by][lz] = 0;
                        chunks[{cx, cz}].isDirty = true;
                        chunks[{cx, cz}].isModified = true;
                        if (lx == 0 && chunks.find({cx-1, cz}) != chunks.end()) chunks[{cx-1, cz}].isDirty = true;
                        if (lx == CHUNK_SIZE-1 && chunks.find({cx+1, cz}) != chunks.end()) chunks[{cx+1, cz}].isDirty = true;
                        if (lz == 0 && chunks.find({cx, cz-1}) != chunks.end()) chunks[{cx, cz-1}].isDirty = true;
                        if (lz == CHUNK_SIZE-1 && chunks.find({cx, cz+1}) != chunks.end()) chunks[{cx, cz+1}].isDirty = true;
                        workerCV.notify_one();
                    }
                }
            }
        }
    } else {
        miningProgress.store(0.0f);
    }

    if (inputPlace.load() && placeCooldown.load() <= 0.0f) {
        if (hit.px == hit.bx && hit.py == hit.by && hit.pz == hit.bz) {
            return;
        }

        uint16_t selectedItem;
        {
            std::lock_guard<std::mutex> lock(inventoryMutex);
            selectedItem = inventory[selectedHotbarSlot + 27].itemId;
        }

        if (selectedItem != 0 && (Registry::getItem(selectedItem).blockId != 0)) {
            int pcx = hit.px / CHUNK_SIZE;
            int pcz = hit.pz / CHUNK_SIZE;
            if (hit.px < 0 && hit.px % CHUNK_SIZE != 0) pcx -= 1;
            if (hit.pz < 0 && hit.pz % CHUNK_SIZE != 0) pcz -= 1;
            int plx = (hit.px % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
            int plz = (hit.pz % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

            if (hit.py >= 0 && hit.py < CHUNK_HEIGHT) {
                glm::vec3 pSize = isCrouching.load() ? glm::vec3(0.6f, 1.5f, 0.6f) : glm::vec3(0.6f, 1.8f, 0.6f);
                float pX = playerX.load();
                float pY = playerY.load();
                float pZ = playerZ.load();
                bool intersectX = abs(pX - (hit.px + 0.5f)) * 2.0f < (pSize.x + 1.0f);
                bool intersectY = abs((pY + pSize.y/2.0f) - (hit.py + 0.5f)) * 2.0f < (pSize.y + 1.0f);
                bool intersectZ = abs(pZ - (hit.pz + 0.5f)) * 2.0f < (pSize.z + 1.0f);
                
                if (!(intersectX && intersectY && intersectZ)) {
                    std::lock_guard<std::mutex> lock(worldMutex);
                    if (chunks.find({pcx, pcz}) != chunks.end()) {
                        chunks[{pcx, pcz}].data[plx][hit.py][plz] = selectedItem;
                        chunks[{pcx, pcz}].isDirty = true;
                        chunks[{pcx, pcz}].isModified = true;
                        if (plx == 0 && chunks.find({pcx-1, pcz}) != chunks.end()) chunks[{pcx-1, pcz}].isDirty = true;
                        if (plx == CHUNK_SIZE-1 && chunks.find({pcx+1, pcz}) != chunks.end()) chunks[{pcx+1, pcz}].isDirty = true;
                        if (plz == 0 && chunks.find({pcx, pcz-1}) != chunks.end()) chunks[{pcx, pcz-1}].isDirty = true;
                        if (plz == CHUNK_SIZE-1 && chunks.find({pcx, pcz+1}) != chunks.end()) chunks[{pcx, pcz+1}].isDirty = true;
                        consumeItem(selectedHotbarSlot + 27, 1);
                        placeCooldown.store(0.2f);
                        workerCV.notify_one();
                    }
                }
            }
        }
    }
}
