#include "World.h"
#include "../gameplay/Registry.h"
#include "../core/Globals.h"
#include "../geometry/GeometryAssetLoader.h"
#include <functional>

void addFace(std::vector<float>& vertices, int x, int y, int z, int faceType, int blockType) {
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    if (faceType == 0) nz = 1.0f; else if (faceType == 1) nz = -1.0f;
    else if (faceType == 2) ny = 1.0f; else if (faceType == 3) ny = -1.0f;
    else if (faceType == 4) nx = 1.0f; else if (faceType == 5) nx = -1.0f;
    float ao = 1.0f;
    float texIndex = (float)blockType;
    float fx = (float)x, fy = (float)y, fz = (float)z;
    auto addV = [&](float vx, float vy, float vz, float u, float v) {
        vertices.insert(vertices.end(), {vx, vy, vz, u, v, texIndex, nx, ny, nz, ao});
    };
    if (faceType == 0) { 
        addV(fx-0.5f, fy-0.5f, fz+0.5f, 0, 1); addV(fx+0.5f, fy-0.5f, fz+0.5f, 1, 1); addV(fx+0.5f, fy+0.5f, fz+0.5f, 1, 0);
        addV(fx+0.5f, fy+0.5f, fz+0.5f, 1, 0); addV(fx-0.5f, fy+0.5f, fz+0.5f, 0, 0); addV(fx-0.5f, fy-0.5f, fz+0.5f, 0, 1);
    } else if (faceType == 1) { 
        addV(fx-0.5f, fy+0.5f, fz-0.5f, 0, 0); addV(fx+0.5f, fy+0.5f, fz-0.5f, 1, 0); addV(fx+0.5f, fy-0.5f, fz-0.5f, 1, 1);
        addV(fx+0.5f, fy-0.5f, fz-0.5f, 1, 1); addV(fx-0.5f, fy-0.5f, fz-0.5f, 0, 1); addV(fx-0.5f, fy+0.5f, fz-0.5f, 0, 0);
    } else if (faceType == 2) { 
        addV(fx-0.5f, fy+0.5f, fz+0.5f, 0, 1); addV(fx+0.5f, fy+0.5f, fz+0.5f, 1, 1); addV(fx+0.5f, fy+0.5f, fz-0.5f, 1, 0);
        addV(fx+0.5f, fy+0.5f, fz-0.5f, 1, 0); addV(fx-0.5f, fy+0.5f, fz-0.5f, 0, 0); addV(fx-0.5f, fy+0.5f, fz+0.5f, 0, 1);
    } else if (faceType == 3) { 
        addV(fx-0.5f, fy-0.5f, fz-0.5f, 0, 0); addV(fx+0.5f, fy-0.5f, fz-0.5f, 1, 0); addV(fx+0.5f, fy-0.5f, fz+0.5f, 1, 1);
        addV(fx+0.5f, fy-0.5f, fz+0.5f, 1, 1); addV(fx-0.5f, fy-0.5f, fz+0.5f, 0, 1); addV(fx-0.5f, fy-0.5f, fz-0.5f, 0, 0);
    } else if (faceType == 4) { 
        addV(fx+0.5f, fy+0.5f, fz+0.5f, 1, 0); addV(fx+0.5f, fy-0.5f, fz+0.5f, 1, 1); addV(fx+0.5f, fy-0.5f, fz-0.5f, 0, 1);
        addV(fx+0.5f, fy-0.5f, fz-0.5f, 0, 1); addV(fx+0.5f, fy+0.5f, fz-0.5f, 0, 0); addV(fx+0.5f, fy+0.5f, fz+0.5f, 1, 0);
    } else if (faceType == 5) { 
        addV(fx-0.5f, fy-0.5f, fz-0.5f, 0, 1); addV(fx-0.5f, fy-0.5f, fz+0.5f, 1, 1); addV(fx-0.5f, fy+0.5f, fz+0.5f, 1, 0);
        addV(fx-0.5f, fy+0.5f, fz+0.5f, 1, 0); addV(fx-0.5f, fy+0.5f, fz-0.5f, 0, 0); addV(fx-0.5f, fy-0.5f, fz-0.5f, 0, 1);
    }
}

void generateChunkData(int cx, int cz, uint8_t tempData[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE]) {
    memset(tempData, 0, CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE);
    if (cx == 0 && cz == 0) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int y = 0; y < 4; ++y) {
                    tempData[x][y][z] = 3; // Stone
                }
                tempData[x][4][z] = 1; // Grass
            }
        }
        // Prototype showcase blocks at y=5
        tempData[2][5][2] = 3; // Stone block
        tempData[4][5][2] = 4; // Oak Stairs (ID 4)
        tempData[6][5][2] = 5; // Torch (ID 5)
        tempData[8][5][2] = 6; // Oak Door lower (ID 6)
        tempData[8][6][2] = 6; // Oak Door upper (ID 6)
    } else {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int y = 0; y < 4; ++y) {
                    tempData[x][y][z] = 3;
                }
                tempData[x][4][z] = 1;
            }
        }
    }
}

void buildGreedyMesh(std::vector<float>& vertices, const std::function<uint8_t(int,int,int)>& getBlock) {
    auto isOpaque = [](uint8_t b) -> bool {
        if (b == 0) return false;
        return Registry::getBlock(b).opaque;
    };

    int dims[3] = {CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE};
    
    for (int d = 0; d < 3; d++) {
        int u = (d + 1) % 3;
        int v = (d + 2) % 3;
        
        int x[3] = {0, 0, 0};
        int q[3] = {0, 0, 0};
        q[d] = 1;
        
        uint16_t mask[CHUNK_SIZE * CHUNK_HEIGHT];
        
        for (x[d] = -1; x[d] < dims[d]; ) {
            int n = 0;
            for (x[v] = 0; x[v] < dims[v]; ++x[v]) {
                for (x[u] = 0; x[u] < dims[u]; ++x[u]) {
                    uint8_t b1 = (x[d] >= 0) ? getBlock(x[0], x[1], x[2]) : 0;
                    uint8_t b2 = getBlock(x[0] + q[0], x[1] + q[1], x[2] + q[2]);
                    
                    if (b1 == b2) {
                        mask[n] = 0;
                    } else {
                        bool draw1 = (b1 != 0);
                        bool draw2 = (b2 != 0);
                        
                        if (draw1 && !draw2) {
                            mask[n] = b1 | (1 << 8); 
                        } else if (!draw1 && draw2) {
                            mask[n] = b2 | (2 << 8); 
                        } else if (draw1 && draw2) {
                            mask[n] = b1 | (1 << 8);
                        } else {
                            mask[n] = 0;
                        }
                    }
                    n++;
                }
            }
            ++x[d];
            n = 0;
            
            for (int j = 0; j < dims[v]; ++j) {
                for (int i = 0; i < dims[u]; ) {
                    if (mask[n] != 0) {
                        int w, h;
                        for (w = 1; i + w < dims[u] && mask[n + w] == mask[n]; ++w) {}
                        
                        bool done = false;
                        for (h = 1; j + h < dims[v]; ++h) {
                            for (int k = 0; k < w; ++k) {
                                if (mask[n + k + h * dims[u]] != mask[n]) {
                                    done = true;
                                    break;
                                }
                            }
                            if (done) break;
                        }
                        
                        x[u] = i;
                        x[v] = j;
                        
                        int du[3] = {0,0,0}; du[u] = w;
                        int dv[3] = {0,0,0}; dv[v] = h;
                        
                        uint8_t blockType = mask[n] & 0xFF;
                        uint8_t dir = mask[n] >> 8;
                        
                        int faceType = 0;
                        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
                        if (d == 0) { faceType = (dir==1) ? 4 : 5; nx = (dir==1) ? 1.0f : -1.0f; }
                        else if (d == 1) { faceType = (dir==1) ? 2 : 3; ny = (dir==1) ? 1.0f : -1.0f; }
                        else if (d == 2) { faceType = (dir==1) ? 0 : 1; nz = (dir==1) ? 1.0f : -1.0f; }
                        
                        float texIndex = (float)Registry::getBlockTexIndex(blockType, faceType);
                        float ao = 1.0f;
                        
                        float v0[3] = {(float)x[0]-0.5f, (float)x[1]-0.5f, (float)x[2]-0.5f};
                        float v1[3] = {(float)(x[0]+du[0])-0.5f, (float)(x[1]+du[1])-0.5f, (float)(x[2]+du[2])-0.5f};
                        float v2[3] = {(float)(x[0]+du[0]+dv[0])-0.5f, (float)(x[1]+du[1]+dv[1])-0.5f, (float)(x[2]+du[2]+dv[2])-0.5f};
                        float v3[3] = {(float)(x[0]+dv[0])-0.5f, (float)(x[1]+dv[1])-0.5f, (float)(x[2]+dv[2])-0.5f};
                        
                        auto addV = [&](float* vec) {
                            float uvX = 0.0f, uvY = 0.0f;
                            if (d == 0) {
                                uvX = (dir == 1) ? -vec[2] : vec[2];
                                uvY = vec[1];
                            } else if (d == 1) {
                                uvX = vec[0];
                                uvY = (dir == 1) ? vec[2] : -vec[2];
                            } else {
                                uvX = (dir == 1) ? vec[0] : -vec[0];
                                uvY = vec[1];
                            }
                            vertices.insert(vertices.end(), {vec[0], vec[1], vec[2], uvX, uvY, texIndex, nx, ny, nz, ao});
                        };
                        
                        bool flip = (dir == 2);
                        
                        if (!flip) {
                            addV(v0); addV(v1); addV(v2); 
                            addV(v2); addV(v3); addV(v0);
                        } else {
                            addV(v0); addV(v3); addV(v2); 
                            addV(v2); addV(v1); addV(v0);
                        }
                        
                        for (int l = 0; l < h; ++l) {
                            for (int k = 0; k < w; ++k) {
                                mask[n + k + l * dims[u]] = 0;
                            }
                        }
                        
                        i += w;
                        n += w;
                    } else {
                        i++;
                        n++;
                    }
                }
            }
        }
    }

    // Append loaded prototype geometry for prototype blocks in chunk
    for (int gx = 0; gx < CHUNK_SIZE; ++gx) {
        for (int gy = 0; gy < CHUNK_HEIGHT; ++gy) {
            for (int gz = 0; gz < CHUNK_SIZE; ++gz) {
                uint8_t b = getBlock(gx, gy, gz);
                if (b != 0 && fincraft::geometry::GeometryManager::hasPrototype(b)) {
                    const auto* proto = fincraft::geometry::GeometryManager::getPrototype(b);
                    if (proto) {
                        for (const auto& comp : proto->components) {
                            const auto& v10f = comp.vertices10f;
                            for (size_t i = 0; i < v10f.size(); i += 10) {
                                float vx = v10f[i + 0] + (float)gx;
                                float vy = v10f[i + 1] + (float)gy;
                                float vz = v10f[i + 2] + (float)gz;
                                float u  = v10f[i + 3];
                                float v  = v10f[i + 4];
                                float tex = v10f[i + 5];
                                float nx = v10f[i + 6];
                                float ny = v10f[i + 7];
                                float nz = v10f[i + 8];
                                float ao = v10f[i + 9];
                                vertices.insert(vertices.end(), {vx, vy, vz, u, v, tex, nx, ny, nz, ao});
                            }
                        }
                    }
                }
            }
        }
    }
}

void doGen(ChunkPos pos) {
    uint8_t tempData[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    generateChunkData(pos.x, pos.z, tempData);
    
    std::lock_guard<std::mutex> lock(worldMutex);
    auto it = chunks.find(pos);
    if (it != chunks.end() && it->second.state == ChunkState::QueuedForGeneration) {
        memcpy(it->second.data, tempData, sizeof(tempData));
        it->second.state = ChunkState::Generated;
        it->second.isDirty = true;
        
        for (int nx = -1; nx <= 1; nx += 2) {
            auto nit = chunks.find({pos.x + nx, pos.z});
            if (nit != chunks.end() && nit->second.state >= ChunkState::Generated) {
                nit->second.isDirty = true;
            }
        }
        for (int nz = -1; nz <= 1; nz += 2) {
            auto nit = chunks.find({pos.x, pos.z + nz});
            if (nit != chunks.end() && nit->second.state >= ChunkState::Generated) {
                nit->second.isDirty = true;
            }
        }
    }
}

void doMesh(ChunkPos pos) {
    uint8_t localData[3][3][CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    bool valid[3][3] = {false};
    
    {
        std::lock_guard<std::mutex> lock(worldMutex);
        auto it = chunks.find(pos);
        if (it == chunks.end() || it->second.state == ChunkState::Unloaded) return;
        it->second.state = ChunkState::Meshing;
        
        for (int nx = -1; nx <= 1; nx++) {
            for (int nz = -1; nz <= 1; nz++) {
                auto nit = chunks.find({pos.x + nx, pos.z + nz});
                if (nit != chunks.end() && nit->second.state >= ChunkState::Generated) {
                    memcpy(localData[nx+1][nz+1], nit->second.data, sizeof(localData[nx+1][nz+1]));
                    valid[nx+1][nz+1] = true;
                }
            }
        }
    }
    
    auto getBlock = [&](int x, int y, int z) -> uint8_t {
        if (y < 0 || y >= CHUNK_HEIGHT) return 0;
        int cx = 1, cz = 1;
        if (x < 0) { cx = 0; x += CHUNK_SIZE; }
        else if (x >= CHUNK_SIZE) { cx = 2; x -= CHUNK_SIZE; }
        if (z < 0) { cz = 0; z += CHUNK_SIZE; }
        else if (z >= CHUNK_SIZE) { cz = 2; z -= CHUNK_SIZE; }
        
        if (valid[cx][cz]) {
            int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
            int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
            return localData[cx][cz][lx][y][lz];
        }
        return 0;
    };
    
    std::vector<float> meshData;
    buildGreedyMesh(meshData, getBlock);
    
    bool shouldUpload = false;
    {
        std::lock_guard<std::mutex> lock(worldMutex);
        auto it = chunks.find(pos);
        if (it != chunks.end() && it->second.state == ChunkState::Meshing) {
            it->second.state = ChunkState::MeshReady;
            shouldUpload = true;
        }
    }
    if (shouldUpload) {
        std::lock_guard<std::mutex> upLock(uploadMutex);
        bool replaced = false;
        for (auto& job : uploadQueue) {
            if (job.pos == pos) {
                job.meshData = std::move(meshData);
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            uploadQueue.push_back({pos, std::move(meshData)});
        }
    }
}

void updateStreaming(int px, int pz) {
    std::vector<GLuint> localVaosToDelete;
    std::vector<GLuint> localVbosToDelete;
    {
        std::lock_guard<std::mutex> lock(worldMutex);
        
        for (auto it = chunks.begin(); it != chunks.end(); ) {
            int dx = it->first.x - px;
            int dz = it->first.z - pz;
            if (dx*dx + dz*dz > UnloadRadius*UnloadRadius) {
                if (it->second.VAO != 0) {
                    localVaosToDelete.push_back(it->second.VAO);
                    localVbosToDelete.push_back(it->second.VBO);
                }
                it = chunks.erase(it);
            } else {
                ++it;
            }
        }
    
    std::vector<std::pair<float, ChunkPos>> toGen;
    std::vector<std::pair<float, ChunkPos>> toMesh;
    
    int lRad = LoadRadius.load();
    int mRad = MeshRadius.load();

    for (int dx = -lRad; dx <= lRad; dx++) {
        for (int dz = -lRad; dz <= lRad; dz++) {
            float distSq = dx*dx + dz*dz;
            if (distSq <= lRad*lRad) {
                ChunkPos pos{px + dx, pz + dz};
                auto it = chunks.find(pos);
                if (it == chunks.end()) {
                    chunks[pos].state = ChunkState::QueuedForGeneration;
                    toGen.push_back({distSq, pos});
                } else if (it->second.state == ChunkState::Unloaded) {
                    it->second.state = ChunkState::QueuedForGeneration;
                    toGen.push_back({distSq, pos});
                } else if (it->second.state == ChunkState::QueuedForGeneration) {
                    toGen.push_back({distSq, pos});
                }
            }
        }
    }
    
    for (int dx = -mRad; dx <= mRad; dx++) {
        for (int dz = -mRad; dz <= mRad; dz++) {
            float distSq = dx*dx + dz*dz;
            if (distSq <= mRad*mRad) {
                ChunkPos pos{px + dx, pz + dz};
                auto it = chunks.find(pos);
                if (it != chunks.end()) {
                    if (it->second.state == ChunkState::Generated || (it->second.isDirty && (it->second.state == ChunkState::GPUResident || it->second.state == ChunkState::MeshReady))) {
                        bool neighborsReady = true;
                        for (int nx = -1; nx <= 1; nx++) {
                            for (int nz = -1; nz <= 1; nz++) {
                                auto nit = chunks.find({pos.x + nx, pos.z + nz});
                                if (nit == chunks.end() || nit->second.state < ChunkState::Generated) {
                                    neighborsReady = false;
                                    break;
                                }
                            }
                            if (!neighborsReady) break;
                        }
                        if (neighborsReady) {
                            it->second.isDirty = false;
                            it->second.state = ChunkState::QueuedForMeshing;
                            toMesh.push_back({distSq, pos});
                        }
                    } else if (it->second.state == ChunkState::QueuedForMeshing) {
                        toMesh.push_back({distSq, pos});
                    }
                }
            }
        }
    }
    
    std::sort(toGen.begin(), toGen.end(), [](auto& a, auto& b) { return a.first > b.first; });
    std::sort(toMesh.begin(), toMesh.end(), [](auto& a, auto& b) { return a.first > b.first; });
    
    std::lock_guard<std::mutex> qLock(queueMutex);
    generateQueue.clear();
    meshQueue.clear();
    for (auto& p : toGen) generateQueue.push_back(p.second);
    for (auto& p : toMesh) meshQueue.push_back(p.second);
    
    if (!generateQueue.empty() || !meshQueue.empty()) workerCV.notify_one();
    }
    
    if (!localVaosToDelete.empty() || !localVbosToDelete.empty()) {
        std::lock_guard<std::mutex> upLock(uploadMutex);
        for (GLuint vao : localVaosToDelete) deleteQueueVAO.push_back(vao);
        for (GLuint vbo : localVbosToDelete) deleteQueueVBO.push_back(vbo);
    }
}

bool isBlockSolid(int x, int y, int z) {
    if (y < 0 || y >= CHUNK_HEIGHT) return false;
    int cx = x / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    if (x < 0 && x % CHUNK_SIZE != 0) cx -= 1;
    if (z < 0 && z % CHUNK_SIZE != 0) cz -= 1;

    int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

    std::lock_guard<std::mutex> lock(worldMutex);
    auto it = chunks.find({cx, cz});
    if (it != chunks.end() && it->second.state >= ChunkState::Generated) {
        uint8_t block = it->second.data[lx][y][lz];
        return block != 0;
    }
    return false;
}
