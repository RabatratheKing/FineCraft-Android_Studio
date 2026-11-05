#include "SaveManager.h"
#include "../core/Globals.h"
#include "../gameplay/Inventory.h"
#include "../gameplay/Registry.h"
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
        
        // Header (Magic: FINC, Version: 3)
        const char magic[4] = {'F','I','N','C'};
        out.write(magic, 4);
        uint32_t version = 3;
        out.write((char*)&version, sizeof(version));
        
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
        
        InventorySlot invCopy[INVENTORY_SIZE];
        InventorySlot craftCopy[4];
        {
            std::lock_guard<std::mutex> lock(inventoryMutex);
            for (int i = 0; i < INVENTORY_SIZE; i++) {
                invCopy[i] = inventory[i];
            }
            for (int i = 0; i < 4; i++) {
                craftCopy[i] = craftingGrid[i];
            }
        }

        for (int i = 0; i < INVENTORY_SIZE; i++) {
            out.write((char*)&invCopy[i].itemId, sizeof(uint16_t));
            out.write((char*)&invCopy[i].count, sizeof(uint16_t));
        }
        out.write((char*)&hotbar, sizeof(hotbar));

        for (int i = 0; i < 4; i++) {
            out.write((char*)&craftCopy[i].itemId, sizeof(uint16_t));
            out.write((char*)&craftCopy[i].count, sizeof(uint16_t));
        }
        
        std::vector<std::pair<ChunkPos, std::vector<StateId>>> chunksToSave;
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            for (const auto& pair : chunks) {
                if (pair.second.isModified && pair.second.state >= ChunkState::Generated) {
                    std::vector<StateId> dataBuf(CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
                    memcpy(dataBuf.data(), pair.second.data, sizeof(pair.second.data));
                    chunksToSave.push_back({pair.first, std::move(dataBuf)});
                }
            }
        }
        
        uint32_t numChunks = chunksToSave.size();
        out.write((char*)&numChunks, sizeof(numChunks));
        
        for (const auto& c : chunksToSave) {
            out.write((char*)&c.first.x, sizeof(int));
            out.write((char*)&c.first.z, sizeof(int));
            out.write((char*)c.second.data(), CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE * sizeof(StateId));
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
            LOGI("Game saved successfully (version 3)");
        }
    }

    bool LoadGame() {
        if (saveFilePath.empty()) {
            NewGame();
            return false;
        }
        
        std::ifstream in(saveFilePath, std::ios::binary);
        if (!in) {
            LOGE("Failed to open save file for reading, starting new game");
            NewGame();
            return false;
        }
        
        char magic[4];
        in.read(magic, 4);
        if (!in || magic[0] != 'F' || magic[1] != 'I' || magic[2] != 'N' || magic[3] != 'C') {
            LOGE("Invalid save file magic, starting new game");
            NewGame();
            return false;
        }
        
        uint32_t version;
        in.read((char*)&version, sizeof(version));
        if (!in || (version < 1 || version > 3)) {
            LOGE("Unsupported save file version: %u, starting new game", version);
            NewGame();
            return false;
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
            LOGE("Corrupt save file reading globals, starting new game");
            NewGame();
            return false;
        }

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
                LOGE("Corrupt save file reading inventory, starting new game");
                NewGame();
                return false;
            }
            if (loadedInv[i].itemId > 5000 || loadedInv[i].count > 64) {
                loadedInv[i] = {0, 0};
            }
        }
        in.read((char*)&loadedHotbar, sizeof(loadedHotbar));
        if (!in) {
            LOGE("Corrupt save file reading hotbar slot, starting new game");
            NewGame();
            return false;
        }
        if (loadedHotbar < 0 || loadedHotbar >= HOTBAR_SIZE) loadedHotbar = 0;

        InventorySlot loadedCraft[4] = {{0,0}, {0,0}, {0,0}, {0,0}};
        if (version >= 2) {
            for (int i = 0; i < 4; i++) {
                in.read((char*)&loadedCraft[i].itemId, sizeof(uint16_t));
                in.read((char*)&loadedCraft[i].count, sizeof(uint16_t));
                if (!in) {
                    LOGE("Corrupt save file reading crafting grid, starting new game");
                    NewGame();
                    return false;
                }
                if (loadedCraft[i].itemId > 5000 || loadedCraft[i].count > 64) {
                    loadedCraft[i] = {0, 0};
                }
            }
        }
        
        uint32_t numChunks;
        in.read((char*)&numChunks, sizeof(numChunks));
        if (!in || numChunks > 1024) {
            LOGE("Corrupt save file: invalid chunk count %u, starting new game", numChunks);
            NewGame();
            return false;
        }

        struct LoadedChunkTemp {
            int cx, cz;
            StateId data[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
        };
        std::vector<LoadedChunkTemp> loadedChunks;
        loadedChunks.reserve(numChunks);

        for (uint32_t i = 0; i < numChunks; i++) {
            int cx, cz;
            in.read((char*)&cx, sizeof(int));
            in.read((char*)&cz, sizeof(int));
            if (!in) {
                LOGE("Corrupt save file reading chunk coords, starting new game");
                NewGame();
                return false;
            }
            if (abs(cx) > 1000 || abs(cz) > 1000) {
                if (version == 3) {
                    in.seekg(CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE * sizeof(StateId), std::ios::cur);
                } else {
                    in.seekg(CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE * sizeof(uint8_t), std::ios::cur);
                }
                continue;
            }
            LoadedChunkTemp cTemp{};
            cTemp.cx = cx;
            cTemp.cz = cz;

            if (version == 3) {
                in.read((char*)cTemp.data, CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE * sizeof(StateId));
            } else {
                uint8_t legacyData[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
                in.read((char*)legacyData, CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE * sizeof(uint8_t));

                // Initialize non-door blocks
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                        for (int z = 0; z < CHUNK_SIZE; ++z) {
                            uint8_t leg = legacyData[x][y][z];
                            if (leg != 6) {
                                cTemp.data[x][y][z] = Registry::getDefaultStateIdForLegacyId(leg);
                            }
                        }
                    }
                }

                // Process oak doors column-by-column (x, z) to detect maximal vertical runs
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int z = 0; z < CHUNK_SIZE; ++z) {
                        int y = 0;
                        while (y < CHUNK_HEIGHT) {
                            if (legacyData[x][y][z] == 6) {
                                int yStart = y;
                                int yEnd = yStart;
                                while (yEnd + 1 < CHUNK_HEIGHT && legacyData[x][yEnd + 1][z] == 6) {
                                    yEnd++;
                                }

                                int runLength = (yEnd - yStart) + 1;
                                for (int offset = 0; offset < runLength; ++offset) {
                                    int currY = yStart + offset;
                                    if (offset % 2 == 0) {
                                        cTemp.data[x][currY][z] = 5666; // Lower half (even offset)
                                    } else {
                                        cTemp.data[x][currY][z] = 5658; // Upper half (odd offset)
                                    }
                                }
                                // Note: Unpaired final blocks (odd run lengths like 3 or 5)
                                // receive lower state (5666) as a deterministic safe fallback.
                                // Original pairing cannot always be uniquely recovered from legacy flat ID data.

                                y = yEnd + 1;
                            } else {
                                y++;
                            }
                        }
                    }
                }
            }

            if (!in) {
                LOGE("Corrupt save file reading chunk data, starting new game");
                NewGame();
                return false;
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
            updateCraftingOutputUnlocked();
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
                memcpy(c.data, cTemp.data, sizeof(c.data));
                c.state = ChunkState::Generated;
                c.isDirty = true;
                c.isModified = true;
            }
        }
        
        LOGI("Game loaded successfully (version %u migrated to v3)", version);
        return true;
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
