#include "Physics.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include "../gameplay/Registry.h"
#include <algorithm>

bool checkCollision(glm::vec3 pos, glm::vec3 size) {
    int minX = floor(pos.x - size.x / 2.0f + 0.001f);
    int maxX = floor(pos.x + size.x / 2.0f - 0.001f);
    int minY = floor(pos.y + 0.001f);
    int maxY = floor(pos.y + size.y - 0.001f);
    int minZ = floor(pos.z - size.z / 2.0f + 0.001f);
    int maxZ = floor(pos.z + size.z / 2.0f - 0.001f);

    glm::vec3 pMin(pos.x - size.x / 2.0f, pos.y, pos.z - size.z / 2.0f);
    glm::vec3 pMax(pos.x + size.x / 2.0f, pos.y + size.y, pos.z + size.z / 2.0f);

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                uint8_t blockId = getBlockAt(glm::vec3(x + 0.5f, (float)y, z + 0.5f));
                if (blockId == 0) continue;

                const auto& boxes = Registry::getCollisionBoxesForBlock(blockId);
                if (boxes.empty()) continue;

                for (const auto& box : boxes) {
                    glm::vec3 bMin(x + box.minX, y + box.minY, z + box.minZ);
                    glm::vec3 bMax(x + box.maxX, y + box.maxY, z + box.maxZ);

                    if (pMin.x < bMax.x && pMax.x > bMin.x &&
                        pMin.y < bMax.y && pMax.y > bMin.y &&
                        pMin.z < bMax.z && pMax.z > bMin.z) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

uint8_t getBlockAt(glm::vec3 pos) {
    int bx = floor(pos.x);
    int by = floor(pos.y);
    int bz = floor(pos.z);
    
    int cx = bx / CHUNK_SIZE;
    int cz = bz / CHUNK_SIZE;
    if (bx < 0 && bx % CHUNK_SIZE != 0) cx -= 1;
    if (bz < 0 && bz % CHUNK_SIZE != 0) cz -= 1;
    
    int lx = (bx % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = (bz % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    
    if (by >= 0 && by < CHUNK_HEIGHT) {
        std::lock_guard<std::mutex> lock(worldMutex);
        if (chunks.find({cx, cz}) != chunks.end() && chunks[{cx, cz}].state >= ChunkState::Generated) {
            return chunks[{cx, cz}].data[lx][by][lz];
        }
    }
    return 0;
}

void applyDamage(float amount, const char* source) {
    if (isDead.load() || isDebugFly.load()) return;
    float dCooldown = damageCooldown.load();
    if (dCooldown > 0.0f) return;
    
    float hp = playerHealth.load() - amount;
    if (hp <= 0.0f) {
        playerHealth.store(0.0f);
        isDead.store(true);
        playerState = LocomotionState::DEAD;
    } else {
        playerHealth.store(hp);
        hurtTime.store(0.5f);
        damageCooldown.store(0.5f);
    }
}

void tickPhysics(float dt) {
    if (dt > 0.1f) dt = 0.1f;
    
    float dCooldown = damageCooldown.load();
    if (dCooldown > 0.0f) damageCooldown.store(dCooldown - dt);
    float hTime = hurtTime.load();
    if (hTime > 0.0f) hurtTime.store(hTime - dt);

    if (isDead.load()) {
        playerState = LocomotionState::DEAD;
        playerVelocity.x = 0;
        playerVelocity.z = 0;
        playerVelocity.y -= 28.0f * dt;
        float pX = playerX.load();
        float pY = playerY.load();
        float pZ = playerZ.load();
        glm::vec3 pos(pX, pY, pZ);
        pos.y += playerVelocity.y * dt;
        glm::vec3 size(0.6f, 0.6f, 0.6f);
        if (checkCollision(pos, size)) {
            if (playerVelocity.y < 0) {
                pos.y = floor(pos.y) + 1.001f;
            }
            playerVelocity.y = 0;
        }
        playerY.store(pos.y);
        
        float eY = cameraEyeY.load();
        eY = glm::mix(eY, 0.2f, 10.0f * dt);
        cameraEyeY.store(eY);
        float bAmt = bobAmount.load();
        bAmt = glm::mix(bAmt, 0.0f, 10.0f * dt);
        bobAmount.store(bAmt);
        return;
    }

    float inMoveX = inputMoveX.load();
    float inMoveY = inputMoveY.load();
    float moveMag = glm::length(glm::vec2(inMoveX, inMoveY));

    glm::vec3 pos(playerX.load(), playerY.load(), playerZ.load());
    uint8_t feetBlock = getBlockAt(pos + glm::vec3(0, 0.1f, 0));
    uint8_t headBlock = getBlockAt(pos + glm::vec3(0, 1.6f, 0));
    
    isInWater.store(feetBlock == 6 || headBlock == 6);
    isSubmerged.store(headBlock == 6);
    isClimbing.store(feetBlock == 10 || headBlock == 10);

    glm::vec3 standingSize = glm::vec3(0.6f, 2.0f, 0.6f);
    glm::vec3 crouchingSize = glm::vec3(0.6f, 1.5f, 0.6f);
    glm::vec3 crawlingSize = glm::vec3(0.6f, 0.6f, 0.6f);
    
    glm::vec3 currentSize = standingSize;
    float targetEyeY = 1.8f;
    bool actualCrouching = false;
    bool actualCrawling = false;

    bool cState = isCrawlingState.load();
    bool crState = isCrouching.load();

    if (cState) {
        if (!checkCollision(pos, crawlingSize)) {
            actualCrawling = true;
            currentSize = crawlingSize;
            targetEyeY = 0.4f;
        } else {
            actualCrawling = true;
            currentSize = crawlingSize;
            targetEyeY = 0.4f;
        }
    } else if (crState) {
        if (!checkCollision(pos, crouchingSize)) {
            actualCrouching = true;
            currentSize = crouchingSize;
            targetEyeY = 1.3f;
        } else {
            actualCrawling = true;
            currentSize = crawlingSize;
            targetEyeY = 0.4f;
        }
    } else {
        if (!checkCollision(pos, standingSize)) {
            currentSize = standingSize;
            targetEyeY = 1.8f;
        } else if (!checkCollision(pos, crouchingSize)) {
            actualCrouching = true;
            currentSize = crouchingSize;
            targetEyeY = 1.3f;
        } else {
            actualCrawling = true;
            currentSize = crawlingSize;
            targetEyeY = 0.4f;
        }
    }
    
    isCrouching.store(actualCrouching);
    isCrawlingState.store(actualCrawling);

    bool sState = isSprinting.load();
    if (actualCrawling || actualCrouching) {
        isSprinting.store(false);
        sState = false;
    }

    bool dbgFly = isDebugFly.load();
    bool inWater = isInWater.load();
    bool clmb = isClimbing.load();
    bool grnd = isGrounded.load();
    bool inJump = inputJump.load();

    if (dbgFly) {
        playerState = LocomotionState::FLYING;
    } else if (inWater) {
        playerState = LocomotionState::SWIMMING;
    } else if (clmb) {
        playerState = LocomotionState::CLIMBING;
    } else if (actualCrawling) {
        playerState = LocomotionState::CRAWLING;
    } else if (actualCrouching) {
        playerState = LocomotionState::CROUCHING;
    } else if (!grnd) {
        if (playerVelocity.y > 0) playerState = LocomotionState::JUMPING;
        else playerState = LocomotionState::FALLING;
    } else if (sState && moveMag > 0.1f) {
        playerState = LocomotionState::SPRINTING;
    } else if (moveMag > 0.1f) {
        playerState = LocomotionState::WALKING;
    } else {
        playerState = LocomotionState::STANDING;
    }

    float cLookX = cameraLookX.load();
    if (playerState == LocomotionState::FLYING) {
        glm::vec3 front;
        front.x = cos(glm::radians(cLookX));
        front.y = 0.0f;
        front.z = sin(glm::radians(cLookX));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        
        float speed = 20.0f * dt;
        if (sState) speed *= 2.0f;
        
        pos.x += (front.x * -inMoveY + right.x * inMoveX) * speed;
        pos.z += (front.z * -inMoveY + right.z * inMoveX) * speed;
        if (inJump) pos.y += speed;
        if (actualCrouching) pos.y -= speed;

        playerX.store(pos.x);
        playerY.store(pos.y);
        playerZ.store(pos.z);
        
        float eY = cameraEyeY.load();
        eY = glm::mix(eY, targetEyeY, 15.0f * dt);
        cameraEyeY.store(eY);

        float bAmt = bobAmount.load();
        bAmt = glm::mix(bAmt, 0.0f, 10.0f * dt);
        bobAmount.store(bAmt);

        float curFOV = currentFOV.load();
        float setFOV = settingFOV.load();
        curFOV = glm::mix(curFOV, setFOV, 10.0f * dt);
        currentFOV.store(curFOV);
        return;
    }

    glm::vec3 front;
    front.x = cos(glm::radians(cLookX));
    front.y = 0.0f;
    front.z = sin(glm::radians(cLookX));
    if (glm::length(front) > 0.001f) front = glm::normalize(front);
    else front = glm::vec3(0,0,1);
    
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 wishDir = front * -inMoveY + right * inMoveX;
    if (glm::length(wishDir) > 1.0f) wishDir = glm::normalize(wishDir);

    float gravity = -28.0f; 
    float jumpVelocity = 9.0f; 
    float currentSpeed = 5.0f;
    float friction = grnd ? 15.0f : 2.5f;

    if (playerState == LocomotionState::SWIMMING) {
        gravity = -4.0f;
        currentSpeed = 3.5f;
        friction = 5.0f;
        playerVelocity.y *= (1.0f - 2.0f * dt);
        if (inJump) playerVelocity.y += 12.0f * dt;
    } else if (playerState == LocomotionState::CLIMBING) {
        gravity = 0.0f;
        currentSpeed = 3.0f;
        friction = 10.0f;
        if (inJump) {
            playerVelocity.y = jumpVelocity * 0.8f;
            isClimbing.store(false);
        } else if (inMoveY < -0.1f) {
            playerVelocity.y = 4.0f;
        } else if (inMoveY > 0.1f) {
            playerVelocity.y = -4.0f;
        } else {
            playerVelocity.y = 0.0f;
        }
    } else if (playerState == LocomotionState::CRAWLING) {
        currentSpeed = 1.5f;
    } else if (playerState == LocomotionState::CROUCHING) {
        currentSpeed = 2.5f;
    } else if (playerState == LocomotionState::SPRINTING) {
        currentSpeed = 8.5f;
    }

    if (playerState != LocomotionState::CLIMBING) {
        playerVelocity.y += gravity * dt;
    }

    if (grnd && inJump && playerState != LocomotionState::SWIMMING && playerState != LocomotionState::CLIMBING) {
        playerVelocity.y = jumpVelocity;
        isGrounded.store(false);
        grnd = false;
    }

    playerVelocity.x = glm::mix(playerVelocity.x, wishDir.x * currentSpeed, friction * dt);
    playerVelocity.z = glm::mix(playerVelocity.z, wishDir.z * currentSpeed, friction * dt);

    float moveX = playerVelocity.x * dt;
    float moveZ = playerVelocity.z * dt;

    if ((actualCrouching || actualCrawling) && grnd && playerState != LocomotionState::CLIMBING && playerState != LocomotionState::SWIMMING) {
        glm::vec3 testPos = pos;
        testPos.x += moveX;
        testPos.y -= 0.1f;
        if (!checkCollision(testPos, currentSize)) {
            moveX = 0;
            playerVelocity.x = 0;
        }
        testPos = pos;
        testPos.z += moveZ;
        testPos.y -= 0.1f;
        if (!checkCollision(testPos, currentSize)) {
            moveZ = 0;
            playerVelocity.z = 0;
        }
    }

    bool steppedUp = false;
    pos.x += moveX;
    if (checkCollision(pos, currentSize)) {
        if (grnd && playerState != LocomotionState::SWIMMING && playerState != LocomotionState::CLIMBING) {
            glm::vec3 stepPos = pos;
            stepPos.y += 1.005f; 
            if (!checkCollision(stepPos, currentSize)) {
                pos = stepPos;
                steppedUp = true;
            } else {
                pos.x -= moveX;
                playerVelocity.x = 0;
            }
        } else {
            pos.x -= moveX;
            playerVelocity.x = 0;
        }
    }
    
    pos.z += moveZ;
    if (checkCollision(pos, currentSize)) {
        if (grnd && !steppedUp && playerState != LocomotionState::SWIMMING && playerState != LocomotionState::CLIMBING) {
            glm::vec3 stepPos = pos;
            stepPos.y += 1.005f;
            if (!checkCollision(stepPos, currentSize)) {
                pos = stepPos;
                steppedUp = true;
            } else {
                pos.z -= moveZ;
                playerVelocity.z = 0;
            }
        } else {
            pos.z -= moveZ;
            playerVelocity.z = 0;
        }
    }

    pos.y += playerVelocity.y * dt;
    if (checkCollision(pos, currentSize)) {
        if (playerVelocity.y < 0) {
            pos.y = floor(pos.y) + 1.001f;
            isGrounded.store(true);
        } else if (playerVelocity.y > 0) {
            pos.y -= playerVelocity.y * dt;
        }
        playerVelocity.y = 0;
    } else {
        isGrounded.store(false);
    }
    
    float highY = highestY.load();
    if (playerState == LocomotionState::SWIMMING || playerState == LocomotionState::CLIMBING || dbgFly) {
        highestY.store(pos.y);
    } else if (!isGrounded.load() && playerVelocity.y < 0) {
        if (pos.y > highY) highestY.store(pos.y);
    } else if (isGrounded.load()) {
        if (highY > pos.y) {
            float fallDist = highY - pos.y;
            if (fallDist >= 3.0f) {
                applyDamage(fallDist - 3.0f, "fall");
            }
        }
        highestY.store(pos.y);
    }

    playerX.store(pos.x);
    playerY.store(pos.y);
    playerZ.store(pos.z);

    float eY = cameraEyeY.load();
    eY = glm::mix(eY, targetEyeY, 15.0f * dt);
    cameraEyeY.store(eY);

    float bTime = bobTime.load();
    float bAmt = bobAmount.load();
    if ((isGrounded.load() || playerState == LocomotionState::SWIMMING) && moveMag > 0.1f && !dbgFly) {
        float bobSpeed = 10.0f;
        if (playerState == LocomotionState::SPRINTING) bobSpeed = 15.0f;
        else if (playerState == LocomotionState::CROUCHING || playerState == LocomotionState::CRAWLING) bobSpeed = 6.0f;
        else if (playerState == LocomotionState::SWIMMING) bobSpeed = 8.0f;
        
        bTime += dt * bobSpeed;
        bAmt = glm::mix(bAmt, 1.0f, 10.0f * dt);
    } else {
        bAmt = glm::mix(bAmt, 0.0f, 10.0f * dt);
    }
    bobTime.store(bTime);
    bobAmount.store(bAmt);

    float curFOV = currentFOV.load();
    float setFOV = settingFOV.load();
    if (playerState == LocomotionState::SPRINTING) {
        curFOV = glm::mix(curFOV, setFOV + 10.0f, 10.0f * dt);
    } else {
        curFOV = glm::mix(curFOV, setFOV, 10.0f * dt);
    }
    currentFOV.store(curFOV);
}
