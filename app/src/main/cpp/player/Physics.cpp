#include "Physics.h"
#include "../core/Globals.h"
#include "../world/World.h"
#include <algorithm>

bool checkCollision(glm::vec3 pos, glm::vec3 size) {
    int minX = floor(pos.x - size.x / 2.0f + 0.001f);
    int maxX = floor(pos.x + size.x / 2.0f - 0.001f);
    int minY = floor(pos.y + 0.001f);
    int maxY = floor(pos.y + size.y - 0.001f);
    int minZ = floor(pos.z - size.z / 2.0f + 0.001f);
    int maxZ = floor(pos.z + size.z / 2.0f - 0.001f);

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                if (isBlockSolid(x, y, z)) {
                    return true;
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
    if (isDead || isDebugFly) return;
    if (damageCooldown > 0.0f) return;
    
    playerHealth -= amount;
    if (playerHealth <= 0.0f) {
        playerHealth = 0.0f;
        isDead = true;
        playerState = LocomotionState::DEAD;
    } else {
        hurtTime = 0.5f;
        damageCooldown = 0.5f;
    }
}

void tickPhysics(float dt) {
    if (dt > 0.1f) dt = 0.1f;
    
    if (damageCooldown > 0.0f) damageCooldown -= dt;
    if (hurtTime > 0.0f) hurtTime -= dt;

    if (isDead) {
        playerState = LocomotionState::DEAD;
        playerVelocity.x = 0;
        playerVelocity.z = 0;
        // Still apply gravity
        playerVelocity.y -= 28.0f * dt;
        glm::vec3 pos(playerX, playerY, playerZ);
        pos.y += playerVelocity.y * dt;
        glm::vec3 size(0.6f, 0.6f, 0.6f); // Crawling size for dead player? or just let them fall
        if (checkCollision(pos, size)) {
            if (playerVelocity.y < 0) {
                pos.y = floor(pos.y) + 1.001f;
            }
            playerVelocity.y = 0;
        }
        playerY = pos.y;
        
        // Lower camera
        cameraEyeY = glm::mix(cameraEyeY, 0.2f, 10.0f * dt);
        bobAmount = glm::mix(bobAmount, 0.0f, 10.0f * dt);
        return;
    }
    float moveMag = glm::length(glm::vec2(inputMoveX, inputMoveY));

    // Determine water and climbing state
    glm::vec3 pos(playerX, playerY, playerZ);
    uint8_t feetBlock = getBlockAt(pos + glm::vec3(0, 0.1f, 0));
    uint8_t headBlock = getBlockAt(pos + glm::vec3(0, 1.6f, 0));
    
    isInWater = (feetBlock == 6 || headBlock == 6);
    isSubmerged = (headBlock == 6);
    isClimbing = (feetBlock == 10 || headBlock == 10); // 10 is scaffolding/ladder

    // Determine target size and state
    glm::vec3 standingSize = glm::vec3(0.6f, 2.0f, 0.6f);
    glm::vec3 crouchingSize = glm::vec3(0.6f, 1.5f, 0.6f);
    glm::vec3 crawlingSize = glm::vec3(0.6f, 0.6f, 0.6f);
    
    glm::vec3 currentSize = standingSize;
    float targetEyeY = 1.8f;
    bool actualCrouching = false;
    bool actualCrawling = false;

    // Resolve crawling/crouching transitions
    if (isCrawlingState) {
        if (!checkCollision(pos, crawlingSize)) {
            actualCrawling = true;
            currentSize = crawlingSize;
            targetEyeY = 0.4f;
        } else {
            actualCrawling = true;
            currentSize = crawlingSize;
            targetEyeY = 0.4f;
        }
    } else if (isCrouching) {
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
    
    // Sync external state to actual state to prevent button desync
    isCrouching = actualCrouching;
    isCrawlingState = actualCrawling;

    if (actualCrawling) isSprinting = false;
    if (actualCrouching) isSprinting = false;

    // Update state machine
    if (isDebugFly) {
        playerState = LocomotionState::FLYING;
    } else if (isInWater) {
        playerState = LocomotionState::SWIMMING;
    } else if (isClimbing) {
        playerState = LocomotionState::CLIMBING;
    } else if (actualCrawling) {
        playerState = LocomotionState::CRAWLING;
    } else if (actualCrouching) {
        playerState = LocomotionState::CROUCHING;
    } else if (!isGrounded) {
        if (playerVelocity.y > 0) playerState = LocomotionState::JUMPING;
        else playerState = LocomotionState::FALLING;
    } else if (isSprinting && moveMag > 0.1f) {
        playerState = LocomotionState::SPRINTING;
    } else if (moveMag > 0.1f) {
        playerState = LocomotionState::WALKING;
    } else {
        playerState = LocomotionState::STANDING;
    }

    if (playerState == LocomotionState::FLYING) {
        glm::vec3 front;
        front.x = cos(glm::radians(cameraLookX));
        front.y = 0.0f;
        front.z = sin(glm::radians(cameraLookX));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        
        float speed = 20.0f * dt;
        if (isSprinting) speed *= 2.0f;
        
        playerX += (front.x * -inputMoveY + right.x * inputMoveX) * speed;
        playerZ += (front.z * -inputMoveY + right.z * inputMoveX) * speed;
        if (inputJump) playerY += speed;
        if (isCrouching) playerY -= speed;
        
        cameraEyeY = glm::mix(cameraEyeY, targetEyeY, 15.0f * dt);
        bobAmount = glm::mix(bobAmount, 0.0f, 10.0f * dt);
        currentFOV = glm::mix(currentFOV, settingFOV, 10.0f * dt);
        return;
    }

    // Kinematics setup
    glm::vec3 front;
    front.x = cos(glm::radians(cameraLookX));
    front.y = 0.0f;
    front.z = sin(glm::radians(cameraLookX));
    if (glm::length(front) > 0.001f) front = glm::normalize(front);
    else front = glm::vec3(0,0,1);
    
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 wishDir = front * -inputMoveY + right * inputMoveX;
    if (glm::length(wishDir) > 1.0f) wishDir = glm::normalize(wishDir);

    float gravity = -28.0f; 
    float jumpVelocity = 9.0f; 
    float currentSpeed = 5.0f;
    float friction = isGrounded ? 15.0f : 2.5f;

    if (playerState == LocomotionState::SWIMMING) {
        gravity = -4.0f;
        currentSpeed = 3.5f;
        friction = 5.0f;
        playerVelocity.y *= (1.0f - 2.0f * dt); // Drag
        if (inputJump) playerVelocity.y += 12.0f * dt;
    } else if (playerState == LocomotionState::CLIMBING) {
        gravity = 0.0f;
        currentSpeed = 3.0f;
        friction = 10.0f;
        if (inputJump) { // jump off
            playerVelocity.y = jumpVelocity * 0.8f;
            isClimbing = false;
        } else if (inputMoveY < -0.1f) {
            playerVelocity.y = 4.0f; // climb up
        } else if (inputMoveY > 0.1f) {
            playerVelocity.y = -4.0f; // climb down
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

    if (isGrounded && inputJump && playerState != LocomotionState::SWIMMING && playerState != LocomotionState::CLIMBING) {
        playerVelocity.y = jumpVelocity;
        isGrounded = false;
    }

    playerVelocity.x = glm::mix(playerVelocity.x, wishDir.x * currentSpeed, friction * dt);
    playerVelocity.z = glm::mix(playerVelocity.z, wishDir.z * currentSpeed, friction * dt);

    float moveX = playerVelocity.x * dt;
    float moveZ = playerVelocity.z * dt;

    // Sneak edge protection
    if ((actualCrouching || actualCrawling) && isGrounded && playerState != LocomotionState::CLIMBING && playerState != LocomotionState::SWIMMING) {
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
        if (moveX != 0 && moveZ != 0) {
            testPos = pos;
            testPos.x += moveX;
            testPos.z += moveZ;
            testPos.y -= 0.1f;
            if (!checkCollision(testPos, currentSize)) {
                moveX = 0;
                moveZ = 0;
                playerVelocity.x = 0;
                playerVelocity.z = 0;
            }
        }
    }

    // X collision and step-up
    bool steppedUp = false;
    pos.x += moveX;
    if (checkCollision(pos, currentSize)) {
        if (isGrounded && playerState != LocomotionState::SWIMMING && playerState != LocomotionState::CLIMBING) {
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
    
    // Z collision and step-up
    pos.z += moveZ;
    if (checkCollision(pos, currentSize)) {
        if (isGrounded && !steppedUp && playerState != LocomotionState::SWIMMING && playerState != LocomotionState::CLIMBING) {
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

    // Y collision
    pos.y += playerVelocity.y * dt;
    if (checkCollision(pos, currentSize)) {
        if (playerVelocity.y < 0) {
            pos.y = floor(pos.y) + 1.001f;
            isGrounded = true;
        } else if (playerVelocity.y > 0) {
            pos.y -= playerVelocity.y * dt; // Simple ceiling bounce
        }
        playerVelocity.y = 0;
    } else {
        isGrounded = false;
    }
    
    // Fall damage logic
    if (playerState == LocomotionState::SWIMMING || playerState == LocomotionState::CLIMBING || isDebugFly) {
        highestY = pos.y;
    } else if (!isGrounded && playerVelocity.y < 0) {
        if (pos.y > highestY) highestY = pos.y;
    } else if (isGrounded) {
        if (highestY > pos.y) {
            float fallDist = highestY - pos.y;
            if (fallDist >= 3.0f) {
                float damage = fallDist - 3.0f;
                applyDamage(damage, "fall");
            }
        }
        highestY = pos.y;
    }

    playerX = pos.x;
    playerY = pos.y;
    playerZ = pos.z;

    // Camera and FOV
    cameraEyeY = glm::mix(cameraEyeY, targetEyeY, 15.0f * dt);

    if ((isGrounded || playerState == LocomotionState::SWIMMING) && moveMag > 0.1f && !isDebugFly) {
        float bobSpeed = 10.0f;
        if (playerState == LocomotionState::SPRINTING) bobSpeed = 15.0f;
        else if (playerState == LocomotionState::CROUCHING || playerState == LocomotionState::CRAWLING) bobSpeed = 6.0f;
        else if (playerState == LocomotionState::SWIMMING) bobSpeed = 8.0f;
        
        bobTime += dt * bobSpeed;
        bobAmount = glm::mix(bobAmount, 1.0f, 10.0f * dt);
    } else {
        bobAmount = glm::mix(bobAmount, 0.0f, 10.0f * dt);
    }

    if (playerState == LocomotionState::SPRINTING) {
        currentFOV = glm::mix(currentFOV, settingFOV + 10.0f, 10.0f * dt);
    } else {
        currentFOV = glm::mix(currentFOV, settingFOV, 10.0f * dt);
    }
}
