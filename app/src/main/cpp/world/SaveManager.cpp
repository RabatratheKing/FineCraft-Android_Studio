#include "SaveManager.h"
#include "../core/Globals.h"
#include "../gameplay/Inventory.h"
#include <fstream>
#include <vector>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
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
        
        // Header
        const char magic[4] = {'F','I','N','C'};
        out.write(magic, 4);
        uint32_t version = 1;
        out.write((char*)&version, sizeof(version));
        
        // Globals
        out.write((char*)&worldSeed, sizeof(worldSeed));
        out.write((char*)&playerX, sizeof(playerX));
        out.write((char*)&playerY, sizeof(playerY));
        out.write((char*)&playerZ, sizeof(playerZ));
        out.write((char*)&cameraLookX, sizeof(cameraLookX));
        out.write((char*)&cameraLookY, sizeof(cameraLookY));
        out.write((char*)&timeOfDay, sizeof(timeOfDay));
        
        clearCraftingGrid();
        // Inventory
        for (int i = 0; i < 36; i++) {
            out.write((char*)&inventory[i].itemId, sizeof(uint16_t));
            out.write((char*)&inventory[i].count, sizeof(uint16_t));
        }
        out.write((char*)&selectedHotbarSlot, sizeof(selectedHotbarSlot));
        
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
        if (magic[0] != 'F' || magic[1] != 'I' || magic[2] != 'N' || magic[3] != 'C') {
            LOGE("Invalid save file magic");
            return;
        }
        
        uint32_t version;
        in.read((char*)&version, sizeof(version));
        if (version != 1) {
            LOGE("Unsupported save file version");
            return;
        }
        
        in.read((char*)&worldSeed, sizeof(worldSeed));
        in.read((char*)&playerX, sizeof(playerX));
        in.read((char*)&playerY, sizeof(playerY));
        in.read((char*)&playerZ, sizeof(playerZ));
        in.read((char*)&cameraLookX, sizeof(cameraLookX));
        in.read((char*)&cameraLookY, sizeof(cameraLookY));
        in.read((char*)&timeOfDay, sizeof(timeOfDay));
        
        for (int i = 0; i < 36; i++) {
            in.read((char*)&inventory[i].itemId, sizeof(uint16_t));
            in.read((char*)&inventory[i].count, sizeof(uint16_t));
        }
        in.read((char*)&selectedHotbarSlot, sizeof(selectedHotbarSlot));
        
        uint32_t numChunks;
        in.read((char*)&numChunks, sizeof(numChunks));
        
        {
            std::lock_guard<std::mutex> qLock(queueMutex);
            generateQueue.clear();
            meshQueue.clear();
        }
        
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            chunks.clear();
            
            for (uint32_t i = 0; i < numChunks; i++) {
                int cx, cz;
                in.read((char*)&cx, sizeof(int));
                in.read((char*)&cz, sizeof(int));
                
                ChunkPos pos{cx, cz};
                Chunk& c = chunks[pos];
                in.read((char*)c.data, CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
                c.state = ChunkState::Generated;
                c.isDirty = true;
                c.isModified = true;
            }
        }
        
        LOGI("Game loaded successfully");
    }

    void NewGame() {
        worldSeed = time(NULL);
        playerX = 0.0f;
        playerY = 16.0f; // Give a bit of height
        playerZ = 0.0f;
        cameraLookX = 0.0f;
        cameraLookY = 0.0f;
        timeOfDay = 0.0f;
        playerHealth = playerMaxHealth;
        isDead = false;
        playerState = LocomotionState::STANDING;
        highestY = playerY;
        
        for (int i = 0; i < 36; i++) {
            inventory[i] = {0, 0};
        }
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
