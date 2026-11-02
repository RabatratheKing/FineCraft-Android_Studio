#include "ChunkManager.h"
#include "../core/Globals.h"
#include "World.h"
#include <chrono>
#include <thread>

void chunkWorkerLoop() {
    int lastPx = -999, lastPz = -999;
    while (isRunning) {
        int px = (int)round(playerX) / CHUNK_SIZE;
        int pz = (int)round(playerZ) / CHUNK_SIZE;
        if (playerX < 0) px -= 1;
        if (playerZ < 0) pz -= 1;
        
        updateStreaming(px, pz);
        lastPx = px;
        lastPz = pz;
        
        ChunkPos genPos, meshPos;
        bool hasGen = false, hasMesh = false;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            workerCV.wait_for(lock, std::chrono::milliseconds(50), [&]{ return !generateQueue.empty() || !meshQueue.empty() || !isRunning; });
            if (!isRunning) break;
            
            if (!meshQueue.empty()) {
                meshPos = meshQueue.back();
                meshQueue.pop_back();
                hasMesh = true;
            } else if (!generateQueue.empty()) {
                genPos = generateQueue.back();
                generateQueue.pop_back();
                hasGen = true;
            }
        }
        
        if (hasMesh) doMesh(meshPos);
        else if (hasGen) doGen(genPos);
    }
}



