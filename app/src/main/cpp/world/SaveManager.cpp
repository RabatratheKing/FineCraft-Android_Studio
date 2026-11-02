#include "SaveManager.h"
#include "../core/Globals.h"
#include "../gameplay/Inventory.h"
#include <fstream>
#include <vector>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Finecraft", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Finecraft", __VA_ARGS__)

namespace SaveManager {
    std::string saveFilePath;

    void Init(const std::string& path) {
        saveFilePath = path + "/world.dat";
    }

    bool HasSave() {
        if (saveFilePath.empty()) return false;
        struct stat buffer;
        return (stat(saveFilePath.c_str(), &buffer) == 0);
    }

    void SaveGame() {
        if (saveFilePath.empty()) return;
        
        std::string tmpPath = saveFilePath + ".tmp";
        std::ofstream out(tmpPath, std::ios::binary);
        if (!out) {
            LOGE("Failed to open save file for writing");
            return;
        }
        
        // Header (Magic: FINC, Version: 2)
        const char magic[4] = {'F','I','N','C'};
        out.write(magic, 4);
        uint32_t version = 2;
        out.write((char*)&version, sizeof(version));
        
        // Globals (thread-safe snapshot loads)
        int seed = worldSeed.load();
        float pX = playerX.load();
        float pY = playerY.load();
        float pZ = playerZ.load();
        float cLookX = cameraLookX.load();
        float cLookY = cameraLookY.load();
        float tOfDay = timeOfDay.load();
        int hotbar = selectedHotbarSlot;

        out.write((char*)&seed, sizeof(seed));
        out.write((char*)&pX, sizeof(pX));
        out.write((char*)&pY, sizeof(pY));
        out.write((char*)&pZ, sizeof(pZ));
        out.write((char*)&cLookX, sizeof(cLookX));
        out.write((char*)&cLookY, sizeof(cLookY));
        out.write((char*)&tOfDay, sizeof(tOfDay));
        
        // Protect inventory snapshot with mutex
        InventorySlot invCopy[INVENTORY_SIZE];
        InventorySlot craftCopy[4];
        {
            std::lock_guard<std::mutex> lock(inventoryMutex);
            clearCraftingGrid(); // Safe non-destructive return to inventory
            for (int i = 0; i < INVENTORY_SIZE; i++) {
                invCopy[i] = inventory[i];
            }
            for (int i = 0; i < 4; i++) {
                craftCopy[i] = craftingGrid[i];
            }
        }

        // Write Inventory (36 slots)
        for (int i = 0; i < INVENTORY_SIZE; i++) {
            out.write((char*)&invCopy[i].itemId, sizeof(uint16_t));
            out.write((char*)&invCopy[i].count, sizeof(uint16_t));
        }
        out.write((char*)&hotbar, sizeof(hotbar));

        // Write Crafting Grid (4 slots) - Version 2 addition
        for (int i = 0; i < 4; i++) {
            out.write((char*)&craftCopy[i].itemId, sizeof(uint16_t));
            out.write((char*)&craftCopy[i].count, sizeof(uint16_t));
        }
        
        // Chunks
        std::vector<std::pair<ChunkPos, std::vector<uint8_t>>> chunksToSave;
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            for (const auto& pair : chunks) {
                if (pair.second.isModified && pair.second.state >= ChunkState::Generated) {
                    std::vector<uint8_t> dataBuf(CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
                    memcpy(dataBuf.data(), pair.second.data, CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
                    chunksToSave.push_back({pair.first, std::move(dataBuf)});
                }
            }
        }
        
        uint32_t numChunks = chunksToSave.size();
        out.write((char*)&numChunks, sizeof(numChunks));
        
        for (const auto& c : chunksToSave) {
            out.write((char*)&c.first.x, sizeof(int));
            out.write((char*)&c.first.z, sizeof(int));
            out.write((char*)c.second.data(), CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
        }
        
        out.flush();
        if (!out.good()) {
            LOGE("Write error during save file generation");
            out.close();
            unlink(tmpPath.c_str());
            return;
        }
        out.close();
        
        if (rename(tmpPath.c_str(), saveFilePath.c_str()) != 0) {
            LOGE("Failed to rename temp save file");
        } else {
            LOGI("Game saved successfully");
        }
    }

    void LoadGame() {
        if (saveFilePath.empty()) return;
        
        std::ifstream in(saveFilePath, std::ios::binary);
        if (!in) {
            LOGE("Failed to open save file for reading");
            return;
        }
        
        char magic[4];
        in.read(magic, 4);
        if (!in || magic[0] != 'F' || magic[1] != 'I' || magic[2] != 'N' || magic[3] != 'C') {
            LOGE("Invalid save file magic");
            return;
        }
        
        uint32_t version;
        in.read((char*)&version, sizeof(version));
        if (!in || (version != 1 && version != 2)) {
            LOGE("Unsupported save file version: %u", version);
            return;
        }

        int loadedSeed;
        float loadedPX, loadedPY, loadedPZ;
        float loadedCLookX, loadedCLookY;
        float loadedTimeOfDay;
        int loadedHotbar;

        in.read((char*)&loadedSeed, sizeof(loadedSeed));
        in.read((char*)&loadedPX, sizeof(loadedPX));
        in.read((char*)&loadedPY, sizeof(loadedPY));
        in.read((char*)&loadedPZ, sizeof(loadedPZ));
        in.read((char*)&loadedCLookX, sizeof(loadedCLookX));
        in.read((char*)&loadedCLookY, sizeof(loadedCLookY));
        in.read((char*)&loadedTimeOfDay, sizeof(loadedTimeOfDay));

        if (!in) {
            LOGE("Corrupt save file reading globals");
            return;
        }

        // Validate coordinates
        if (std::isnan(loadedPX) || std::isnan(loadedPY) || std::isnan(loadedPZ) || loadedPY < -50.0f || loadedPY > 500.0f) {
            loadedPX = 0.0f; loadedPY = 16.0f; loadedPZ = 0.0f;
        }
        if (std::isnan(loadedCLookX)) loadedCLookX = 0.0f;
        if (std::isnan(loadedCLookY)) loadedCLookY = 0.0f;
        if (std::isnan(loadedTimeOfDay)) loadedTimeOfDay = 0.0f;

        InventorySlot loadedInv[INVENTORY_SIZE];
        for (int i = 0; i < INVENTORY_SIZE; i++) {
            in.read((char*)&loadedInv[i].itemId, sizeof(uint16_t));
            in.read((char*)&loadedInv[i].count, sizeof(uint16_t));
            if (!in) {
                LOGE("Corrupt save file reading inventory");
                return;
            }
            if (loadedInv[i].itemId > 5000 || loadedInv[i].count > 64) {
                loadedInv[i] = {0, 0};
            }
        }
        in.read((char*)&loadedHotbar, sizeof(loadedHotbar));
        if (!in) {
            LOGE("Corrupt save file reading hotbar slot");
            return;
        }
        if (loadedHotbar < 0 || loadedHotbar >= HOTBAR_SIZE) loadedHotbar = 0;

        InventorySlot loadedCraft[4] = {{0,0}, {0,0}, {0,0}, {0,0}};
        if (version >= 2) {
            for (int i = 0; i < 4; i++) {
                in.read((char*)&loadedCraft[i].itemId, sizeof(uint16_t));
                in.read((char*)&loadedCraft[i].count, sizeof(uint16_t));
                if (!in) {
                    LOGE("Corrupt save file reading crafting grid");
                    return;
                }
                if (loadedCraft[i].itemId > 5000 || loadedCraft[i].count > 64) {
                    loadedCraft[i] = {0, 0};
                }
            }
        }
        
        uint32_t numChunks;
        in.read((char*)&numChunks, sizeof(numChunks));
        if (!in || numChunks > 1024) {
            LOGE("Corrupt save file: invalid chunk count %u", numChunks);
            return;
        }

        struct LoadedChunkTemp {
            int cx, cz;
            uint8_t data[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
        };
        std::vector<LoadedChunkTemp> loadedChunks;
        loadedChunks.reserve(numChunks);

        for (uint32_t i = 0; i < numChunks; i++) {
            int cx, cz;
            in.read((char*)&cx, sizeof(int));
            in.read((char*)&cz, sizeof(int));
            if (!in) {
                LOGE("Corrupt save file reading chunk coords");
                return;
            }
            if (abs(cx) > 1000 || abs(cz) > 1000) {
                // Skip invalid chunk
                in.seekg(CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE, std::ios::cur);
                continue;
            }
            LoadedChunkTemp cTemp;
            cTemp.cx = cx;
            cTemp.cz = cz;
            in.read((char*)cTemp.data, CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
            if (!in) {
                LOGE("Corrupt save file reading chunk data");
                return;
            }
            loadedChunks.push_back(cTemp);
        }

        // Successfully validated! Now apply to global state
        worldSeed.store(loadedSeed);
        playerX.store(loadedPX);
        playerY.store(loadedPY);
        playerZ.store(loadedPZ);
        cameraLookX.store(loadedCLookX);
        cameraLookY.store(loadedCLookY);
        timeOfDay.store(loadedTimeOfDay);
        selectedHotbarSlot = loadedHotbar;

        {
            std::lock_guard<std::mutex> lock(inventoryMutex);
            for (int i = 0; i < INVENTORY_SIZE; i++) {
                inventory[i] = loadedInv[i];
            }
            for (int i = 0; i < 4; i++) {
                craftingGrid[i] = loadedCraft[i];
            }
            updateCraftingOutput();
            inventoryVersion++;
        }

        {
            std::lock_guard<std::mutex> qLock(queueMutex);
            generateQueue.clear();
            meshQueue.clear();
        }
        
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            chunks.clear();
            
            for (const auto& cTemp : loadedChunks) {
                ChunkPos pos{cTemp.cx, cTemp.cz};
                Chunk& c = chunks[pos];
                memcpy(c.data, cTemp.data, CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
                c.state = ChunkState::Generated;
                c.isDirty = true;
                c.isModified = true;
            }
        }
        
        LOGI("Game loaded successfully (version %u)", version);
    }

    void NewGame() {
        worldSeed.store(time(NULL));
        playerX.store(0.0f);
        playerY.store(16.0f);
        playerZ.store(0.0f);
        cameraLookX.store(0.0f);
        cameraLookY.store(0.0f);
        timeOfDay.store(0.0f);
        playerHealth.store(20.0f);
        playerMaxHealth.store(20.0f);
        isDead.store(false);
        playerState = LocomotionState::STANDING;
        highestY.store(playerY.load());
        
        initInventory();
        selectedHotbarSlot = 0;
        
        {
            std::lock_guard<std::mutex> qLock(queueMutex);
            generateQueue.clear();
            meshQueue.clear();
        }
        
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            chunks.clear();
        }
        
        SaveGame();
        LOGI("New game started");
    }
}
