#include <jni.h>
#include <string>
#include <android/native_window_jni.h>
#include "core/Globals.h"
#include "world/SaveManager.h"
#include "core/Logger.h"
#include "gameplay/Inventory.h"
#include "player/Physics.h"
#include "gameplay/Raycast.h"

#include "rendering/Renderer.h"
#include "world/ChunkManager.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

AAssetManager* globalAssetManager = nullptr;


extern "C" JNIEXPORT jstring JNICALL
Java_com_example_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from FineCraft C++ Engine";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSurfaceCreated(JNIEnv* env, jclass clazz, jobject surfaceObj) {
    {
        std::unique_lock<std::mutex> lock(windowMutex);
        if (currentWindow) {
            ANativeWindow_release(currentWindow);
        }
        currentWindow = ANativeWindow_fromSurface(env, surfaceObj);
        windowChanged = true;
    }
    
    if (!isRunning) {
        initInventory();
        isRunning = true;
        renderThread = std::thread(renderLoop);
        workerThread = std::thread(chunkWorkerLoop);
    }
    LOGI("Surface Created");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSurfaceChanged(JNIEnv* env, jclass clazz, jint w, jint h) {
    width = w;
    height = h;
    LOGI("Surface Changed: %dx%d", width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSurfaceDestroyed(JNIEnv* env, jclass clazz) {
    {
        std::unique_lock<std::mutex> lock(windowMutex);
        if (currentWindow) {
            ANativeWindow_release(currentWindow);
            currentWindow = nullptr;
        }
        windowChanged = true;
        while (windowChanged && isRunning) {
            windowCV.wait_for(lock, std::chrono::milliseconds(50));
        }
    }
    LOGI("Surface Destroyed");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeCameraLook(JNIEnv* env, jclass clazz, jfloat dx, jfloat dy) {
    float multY = settingInvertY ? -1.0f : 1.0f;
    if (!isInventoryOpen) {
        cameraLookX += dx * 0.5f * settingSensitivity;
        cameraLookY -= dy * 0.5f * settingSensitivity * multY;
    }
    if (cameraLookY > 89.0f) cameraLookY = 89.0f;
    if (cameraLookY < -89.0f) cameraLookY = -89.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeMoveJoystick(JNIEnv* env, jclass clazz, jfloat x, jfloat y) {
    if (!isInventoryOpen) {
        inputMoveX = x;
        inputMoveY = y;
    } else {
        inputMoveX = 0.0f;
        inputMoveY = 0.0f;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeAction(JNIEnv* env, jclass clazz, jstring action) {
    const char *actionStr = env->GetStringUTFChars(action, nullptr);
    if (!actionStr) return;
    size_t actionLen = strlen(actionStr);

    if (isInventoryOpen) {
        // Only allow select slot when inventory is open, or nothing
        if (actionLen >= 13 && strncmp(actionStr, "select_slot_", 12) == 0) {
            int slot = actionStr[12] - '0';
            if (slot >= 0 && slot < HOTBAR_SIZE) {
                selectedHotbarSlot = slot;
            }
        }
        env->ReleaseStringUTFChars(action, actionStr);
        return;
    }
    
    if (strcmp(actionStr, "jump_down") == 0) {
        inputJump = true;
    } else if (strcmp(actionStr, "jump_up") == 0) {
        inputJump = false;
    } else if (strcmp(actionStr, "sprint_down") == 0) {
        isSprinting = true;
    } else if (strcmp(actionStr, "sprint_up") == 0) {
        isSprinting = false;
    } else if (strcmp(actionStr, "sneak") == 0) {
        if (!isCrouching && !isCrawlingState) {
            isCrouching = true;
        } else if (isCrouching && !isCrawlingState) {
            isCrouching = false;
            isCrawlingState = true;
        } else {
            // trying to stand from crawling
            isCrouching = false;
            isCrawlingState = false;
        }
    } else if (strcmp(actionStr, "debug_fly") == 0) {
        isDebugFly = !isDebugFly;
        playerVelocity = glm::vec3(0.0f);
    } else if (strcmp(actionStr, "place") == 0 || strcmp(actionStr, "place_down") == 0) {
        inputPlace = true;
    } else if (strcmp(actionStr, "place_up") == 0) {
        inputPlace = false;
    } else if (strcmp(actionStr, "break") == 0 || strcmp(actionStr, "break_down") == 0) {
        inputBreak = true;
    } else if (strcmp(actionStr, "break_up") == 0) {
        inputBreak = false;
    } else if (actionLen >= 13 && strncmp(actionStr, "select_slot_", 12) == 0) {
        int slot = actionStr[12] - '0';
        if (slot >= 0 && slot < HOTBAR_SIZE) {
            selectedHotbarSlot = slot;
        }
    }
    LOGI("Action: %s", actionStr);
    env->ReleaseStringUTFChars(action, actionStr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeUpdateSettings(JNIEnv* env, jclass clazz, jfloat fov, jfloat sensitivity, jboolean invertY, jint renderDist, jint graphicsQuality, jboolean shadows, jboolean clouds, jboolean fog, jfloat brightness, jboolean viewBobbing) {
    settingFOV = fov;
    settingSensitivity = sensitivity;
    settingInvertY = invertY;
    settingRenderDistance = renderDist;
    settingGraphicsQuality = graphicsQuality;
    settingShadows = shadows;
    settingClouds = clouds;
    settingFog = fog;
    settingBrightness = brightness;
    settingViewBobbing = viewBobbing;

    RenderRadius = renderDist;
    MeshRadius = renderDist + 1;
    LoadRadius = renderDist + 2;
    UnloadRadius = renderDist + 4;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_MainActivity_nativeGetInventory(JNIEnv* env, jclass clazz) {
    jintArray result = env->NewIntArray(INVENTORY_SIZE * 2);
    jint fill[INVENTORY_SIZE * 2];
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        fill[i*2] = inventory[i].itemId;
        fill[i*2+1] = inventory[i].count;
    }
    env->SetIntArrayRegion(result, 0, INVENTORY_SIZE * 2, fill);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSwapSlots(JNIEnv* env, jclass clazz, jint slotA, jint slotB) {
    swapSlots(slotA, slotB);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_MainActivity_nativeGetSelectedHotbarSlot(JNIEnv* env, jclass clazz) {
    return selectedHotbarSlot;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSetInventoryOpen(JNIEnv* env, jclass clazz, jboolean open) {
    isInventoryOpen = open;
}


extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeInitSave(JNIEnv* env, jclass clazz, jstring path) {
    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    SaveManager::Init(pathStr);
    env->ReleaseStringUTFChars(path, pathStr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeInit(JNIEnv* env, jclass clazz, jobject assetManager, jstring dataDir, jstring cacheDir) {
    if (assetManager != nullptr) {
        globalAssetManager = AAssetManager_fromJava(env, assetManager);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_MainActivity_nativeHasSave(JNIEnv* env, jclass clazz) {
    return SaveManager::HasSave();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeLoadGame(JNIEnv* env, jclass clazz) {
    SaveManager::LoadGame();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeNewGame(JNIEnv* env, jclass clazz) {
    SaveManager::NewGame();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSaveGame(JNIEnv* env, jclass clazz) {
    SaveManager::SaveGame();
}


extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSwapCraftingSlot(JNIEnv* env, jclass clazz, jint invSlot, jint craftSlot) {
    swapCraftingSlot(invSlot, craftSlot);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSwapCraftingToCrafting(JNIEnv* env, jclass clazz, jint craftSlotA, jint craftSlotB) {
    swapCraftingToCrafting(craftSlotA, craftSlotB);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeTakeCraftingOutput(JNIEnv* env, jclass clazz) {
    takeCraftingOutput();
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_MainActivity_nativeGetCraftingGrid(JNIEnv* env, jclass clazz) {
    jintArray result = env->NewIntArray(4 * 2);
    jint temp[8];
    for (int i = 0; i < 4; i++) {
        temp[i*2] = craftingGrid[i].itemId;
        temp[i*2+1] = craftingGrid[i].count;
    }
    env->SetIntArrayRegion(result, 0, 8, temp);
    return result;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_MainActivity_nativeGetCraftingOutput(JNIEnv* env, jclass clazz) {
    jintArray result = env->NewIntArray(2);
    jint temp[2] = { craftingOutput.itemId, craftingOutput.count };
    env->SetIntArrayRegion(result, 0, 2, temp);
    return result;
}

extern uint32_t atlasPixelsARGB[256 * 256];
extern bool atlasGenerated;

uint32_t* externalAtlasPixels = nullptr;
int externalAtlasWidth = 256;
int externalAtlasHeight = 256;
bool useExternalAtlas = false;

extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_MainActivity_nativeGetAtlasPixels(JNIEnv* env, jclass clazz) {
    if (useExternalAtlas && externalAtlasPixels != nullptr) {
        jintArray result = env->NewIntArray(externalAtlasWidth * externalAtlasHeight);
        env->SetIntArrayRegion(result, 0, externalAtlasWidth * externalAtlasHeight, (const jint*)externalAtlasPixels);
        return result;
    }
    if (!atlasGenerated) return nullptr;
    jintArray result = env->NewIntArray(256 * 256);
    env->SetIntArrayRegion(result, 0, 256 * 256, (const jint*)atlasPixelsARGB);
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_MainActivity_nativeGetAtlasWidth(JNIEnv* env, jclass clazz) {
    if (useExternalAtlas) return externalAtlasWidth;
    return 256;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_MainActivity_nativeGetAtlasHeight(JNIEnv* env, jclass clazz) {
    if (useExternalAtlas) return externalAtlasHeight;
    return 256;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_MainActivity_nativeMoveItems(JNIEnv* env, jclass clazz, jint srcType, jint srcSlot, jint destType, jint destSlot, jint amount) {
    return moveItems(srcType, srcSlot, destType, destSlot, amount);
}

extern "C" JNIEXPORT jfloat JNICALL Java_com_example_MainActivity_nativeGetHealth(JNIEnv* env, jclass clazz) {
    return playerHealth;
}
extern "C" JNIEXPORT jfloat JNICALL Java_com_example_MainActivity_nativeGetMaxHealth(JNIEnv* env, jclass clazz) {
    return playerMaxHealth;
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_MainActivity_nativeIsDead(JNIEnv* env, jclass clazz) {
    return isDead;
}
extern "C" JNIEXPORT jfloat JNICALL Java_com_example_MainActivity_nativeGetHurtTime(JNIEnv* env, jclass clazz) {
    return hurtTime;
}
extern "C" JNIEXPORT void JNICALL Java_com_example_MainActivity_nativeRespawn(JNIEnv* env, jclass clazz) {
    playerHealth = playerMaxHealth;
    isDead = false;
    playerVelocity = glm::vec3(0.0f);
    playerState = LocomotionState::STANDING;
    isCrouching = false;
    isSprinting = false;
    isCrawlingState = false;
    isInWater = false;
    isSubmerged = false;
    isClimbing = false;
    isGrounded = false;
    
    // Explicit camera reset
    cameraEyeY = 1.8f;
    bobAmount = 0.0f;
    bobTime = 0.0f;

    // Reset damage/fall state
    damageCooldown = 1.0f;
    hurtTime = 0.0f;
    fallDistance = 0.0f;

    // Default spawn near origin
    playerX = 0.5f;
    playerZ = 0.5f;
    
    int spawnY = 15; // default height at (0,0) is usually 14
    
    // If chunk at 0,0 is loaded, find true highest block to prevent spawning inside structures
    {
        std::lock_guard<std::mutex> lock(worldMutex);
        if (chunks.find({0, 0}) != chunks.end() && chunks[{0, 0}].state >= ChunkState::Generated) {
            for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
                uint8_t b = chunks[{0, 0}].data[0][y][0];
                if (b != 0 && b != 6 && b != 10) { // Not air, water, scaffolding
                    spawnY = y + 1;
                    break;
                }
            }
        }
    }
    
    playerY = (float)spawnY + 0.01f;
    highestY = playerY;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSetAtlasPixels(JNIEnv* env, jclass clazz, jintArray pixels, jint width, jint height) {
    if (externalAtlasPixels) delete[] externalAtlasPixels;
    externalAtlasPixels = new uint32_t[width * height];
    externalAtlasWidth = width;
    externalAtlasHeight = height;
    env->GetIntArrayRegion(pixels, 0, width * height, (jint*)externalAtlasPixels);
    useExternalAtlas = true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_MainActivity_nativeIsUsingExternalAtlas(JNIEnv* env, jclass clazz) {
    return useExternalAtlas;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_MainActivity_nativeGetItemSprite(JNIEnv* env, jclass clazz, jint itemId) {
    return Registry::getItemSprite(itemId);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSetDebugWorld(JNIEnv* env, jclass clazz, jboolean isDebug) {
    settingDebugWorld = isDebug;
}
