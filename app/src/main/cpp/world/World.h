#pragma once
#include "../core/Globals.h"
#include <vector>
#include <functional>

void addFace(std::vector<float>& vertices, int x, int y, int z, int faceType, int blockType);
void generateChunkData(int cx, int cz, StateId tempData[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE]);
void buildGreedyMesh(std::vector<float>& vertices, const std::function<StateId(int,int,int)>& getBlock);
void doGen(ChunkPos pos);
void doMesh(ChunkPos pos);
void updateStreaming(int px, int pz);
bool isBlockSolid(int x, int y, int z);
