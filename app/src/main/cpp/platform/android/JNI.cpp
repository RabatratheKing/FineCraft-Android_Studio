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
        windowCV.notify_all();
    }
    
    if (!isRunning.load()) {
        isRunning = true;
        isSimulationActive = true;
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
        windowCV.notify_all();
    }
    LOGI("Surface Destroyed");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeShutdown(JNIEnv* env, jclass clazz) {
    if (isRunning.load()) {
        isRunning = false;
        isSimulationActive = false;
        {
            std::unique_lock<std::mutex> lock(windowMutex);
            if (currentWindow) {
                ANativeWindow_release(currentWindow);
                currentWindow = nullptr;
            }
            windowChanged = true;
            windowCV.notify_all();
        }
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            workerCV.notify_all();
        }
        if (renderThread.joinable()) {
            renderThread.join();
        }
        if (workerThread.joinable()) {
            workerThread.join();
        }
        LOGI("Native Engine Shutdown Complete");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSetSimulationActive(JNIEnv* env, jclass clazz, jboolean active) {
    isSimulationActive.store(active);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeCameraLook(JNIEnv* env, jclass clazz, jfloat dx, jfloat dy) {
    float multY = settingInvertY.load() ? -1.0f : 1.0f;
    if (!isInventoryOpen.load()) {
        cameraLookX.store(cameraLookX.load() + dx * 0.5f * settingSensitivity.load());
        float newY = cameraLookY.load() - dy * 0.5f * settingSensitivity.load() * multY;
        if (newY > 89.0f) newY = 89.0f;
        if (newY < -89.0f) newY = -89.0f;
        cameraLookY.store(newY);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeMoveJoystick(JNIEnv* env, jclass clazz, jfloat x, jfloat y) {
    if (!isInventoryOpen.load()) {
        inputMoveX.store(x);
        inputMoveY.store(y);
    } else {
        inputMoveX.store(0.0f);
        inputMoveY.store(0.0f);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeAction(JNIEnv* env, jclass clazz, jstring action) {
    const char *actionStr = env->GetStringUTFChars(action, nullptr);
    if (!actionStr) return;
    size_t actionLen = strlen(actionStr);

    if (isInventoryOpen.load()) {
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
        inputJump.store(true);
    } else if (strcmp(actionStr, "jump_up") == 0) {
        inputJump.store(false);
    } else if (strcmp(actionStr, "sprint_down") == 0) {
        isSprinting.store(true);
    } else if (strcmp(actionStr, "sprint_up") == 0) {
        isSprinting.store(false);
    } else if (strcmp(actionStr, "sneak") == 0) {
        bool crouching = isCrouching.load();
        bool crawling = isCrawlingState.load();
        if (!crouching && !crawling) {
            isCrouching.store(true);
        } else if (crouching && !crawling) {
            isCrouching.store(false);
            isCrawlingState.store(true);
        } else {
            isCrouching.store(false);
            isCrawlingState.store(false);
        }
    } else if (strcmp(actionStr, "debug_fly") == 0) {
        isDebugFly.store(!isDebugFly.load());
        playerVelocity = glm::vec3(0.0f);
    } else if (strcmp(actionStr, "place") == 0 || strcmp(actionStr, "place_down") == 0) {
        inputPlace.store(true);
    } else if (strcmp(actionStr, "place_up") == 0) {
        inputPlace.store(false);
    } else if (strcmp(actionStr, "break") == 0 || strcmp(actionStr, "break_down") == 0) {
        inputBreak.store(true);
    } else if (strcmp(actionStr, "break_up") == 0) {
        inputBreak.store(false);
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
    settingFOV.store(fov);
    settingSensitivity.store(sensitivity);
    settingInvertY.store(invertY);
    settingRenderDistance.store(renderDist);
    settingGraphicsQuality.store(graphicsQuality);
    settingShadows.store(shadows);
    settingClouds.store(clouds);
    settingFog.store(fog);
    settingBrightness.store(brightness);
    settingViewBobbing.store(viewBobbing);

    LoadRadius = renderDist + 3;
    MeshRadius = renderDist + 2;
    RenderRadius = renderDist;
    UnloadRadius = renderDist + 5;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_MainActivity_nativeGetInventory(JNIEnv* env, jclass clazz) {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    jintArray result = env->NewIntArray(INVENTORY_SIZE * 2);
    if (!result) return nullptr;
    jint fill[INVENTORY_SIZE * 2];
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        fill[i*2] = inventory[i].itemId;
        fill[i*2+1] = inventory[i].count;
    }
    env->SetIntArrayRegion(result, 0, INVENTORY_SIZE * 2, fill);
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_MainActivity_nativeGetInventoryVersion(JNIEnv* env, jclass clazz) {
    return inventoryVersion.load();
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
    isInventoryOpen.store(open);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeInitSave(JNIEnv* env, jclass clazz, jstring path) {
    if (!path) return;
    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    if (pathStr) {
        SaveManager::Init(pathStr);
        env->ReleaseStringUTFChars(path, pathStr);
    }
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
    std::lock_guard<std::mutex> lock(inventoryMutex);
    jintArray result = env->NewIntArray(4 * 2);
    if (!result) return nullptr;
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
    std::lock_guard<std::mutex> lock(inventoryMutex);
    jintArray result = env->NewIntArray(2);
    if (!result) return nullptr;
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
        if (!result) return nullptr;
        env->SetIntArrayRegion(result, 0, externalAtlasWidth * externalAtlasHeight, (const jint*)externalAtlasPixels);
        return result;
    }
    if (!atlasGenerated) return nullptr;
    jintArray result = env->NewIntArray(256 * 256);
    if (!result) return nullptr;
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
    return playerHealth.load();
}
extern "C" JNIEXPORT jfloat JNICALL Java_com_example_MainActivity_nativeGetMaxHealth(JNIEnv* env, jclass clazz) {
    return playerMaxHealth.load();
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_MainActivity_nativeIsDead(JNIEnv* env, jclass clazz) {
    return isDead.load();
}
extern "C" JNIEXPORT jfloat JNICALL Java_com_example_MainActivity_nativeGetHurtTime(JNIEnv* env, jclass clazz) {
    return hurtTime.load();
}
extern "C" JNIEXPORT void JNICALL Java_com_example_MainActivity_nativeRespawn(JNIEnv* env, jclass clazz) {
    playerHealth.store(playerMaxHealth.load());
    isDead.store(false);
    playerVelocity = glm::vec3(0.0f);
    playerState = LocomotionState::STANDING;
    isCrouching.store(false);
    isSprinting.store(false);
    isCrawlingState.store(false);
    isInWater.store(false);
    isSubmerged.store(false);
    isClimbing.store(false);
    isGrounded.store(false);
    
    cameraEyeY.store(1.8f);
    bobAmount.store(0.0f);
    bobTime.store(0.0f);

    damageCooldown.store(1.0f);
    hurtTime.store(0.0f);
    fallDistance.store(0.0f);

    playerX.store(0.5f);
    playerZ.store(0.5f);
    
    int spawnY = 15;
    {
        std::lock_guard<std::mutex> lock(worldMutex);
        if (chunks.find({0, 0}) != chunks.end() && chunks[{0, 0}].state >= ChunkState::Generated) {
            for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
                uint8_t b = chunks[{0, 0}].data[0][y][0];
                if (b != 0 && b != 6 && b != 10) {
                    spawnY = y + 1;
                    break;
                }
            }
        }
    }
    
    playerY.store((float)spawnY + 0.01f);
    highestY.store(playerY.load());
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_nativeSetAtlasPixels(JNIEnv* env, jclass clazz, jintArray pixels, jint w, jint h) {
    if (!pixels || w <= 0 || h <= 0) {
        LOGE("Invalid parameters passed to nativeSetAtlasPixels");
        return;
    }
    jsize len = env->GetArrayLength(pixels);
    int64_t total = (int64_t)w * (int64_t)h;
    if (len < total || total > 8192 * 8192) {
        LOGE("Atlas dimensions exceed limits or array length mismatch: w=%d, h=%d, len=%d", w, h, len);
        return;
    }
    if (externalAtlasPixels) {
        delete[] externalAtlasPixels;
        externalAtlasPixels = nullptr;
    }
    externalAtlasPixels = new (std::nothrow) uint32_t[total];
    if (!externalAtlasPixels) {
        LOGE("Failed to allocate memory for externalAtlasPixels");
        return;
    }
    externalAtlasWidth = w;
    externalAtlasHeight = h;
    env->GetIntArrayRegion(pixels, 0, total, (jint*)externalAtlasPixels);
    useExternalAtlas = true;
    LOGI("Successfully registered external atlas %dx%d", w, h);
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
    settingDebugWorld.store(isDebug);
}
