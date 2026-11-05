#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_map>
#include <thread>
#include <android/native_window.h>
#include <glm/glm.hpp>
#include <GLES3/gl3.h>

extern std::atomic<bool> isRunning;
extern std::atomic<bool> isSimulationActive;
extern bool windowChanged;
extern std::mutex windowMutex;
extern std::condition_variable windowCV;
extern std::thread renderThread;
extern std::thread workerThread;
extern ANativeWindow* currentWindow;
extern int width;
extern int height;

const int CHUNK_SIZE = 16;
const int CHUNK_HEIGHT = 32;
using StateId = uint16_t;

struct ChunkPos {
    int x, z;
    bool operator==(const ChunkPos& other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkPosHash {
    std::size_t operator()(const ChunkPos& k) const {
        return std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 1);
    }
};

extern std::atomic<bool> isDebugFly;
extern std::atomic<bool> isInventoryOpen;
extern glm::vec3 playerVelocity;
extern std::atomic<bool> isGrounded;
extern std::atomic<bool> isCrouching;
extern std::atomic<bool> isSprinting;
extern std::atomic<float> inputMoveX;
extern std::atomic<float> inputMoveY;
extern std::atomic<bool> inputJump;

enum class ChunkState {
    Unloaded,
    QueuedForGeneration,
    Generated,
    QueuedForMeshing,
    Meshing,
    MeshReady,
    GPUResident
};

struct Chunk {
    ChunkState state = ChunkState::Unloaded;
    StateId data[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    GLuint VAO = 0;
    GLuint VBO = 0;
    int vertexCount = 0;
    bool isDirty = false;
    bool isModified = false;
};

extern std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;
extern std::mutex worldMutex;

extern std::mutex queueMutex;
extern std::condition_variable workerCV;
extern std::vector<ChunkPos> generateQueue;
extern std::vector<ChunkPos> meshQueue;

struct GPUUploadJob {
    ChunkPos pos;
    std::vector<float> meshData;
};
extern std::mutex uploadMutex;
extern std::vector<GPUUploadJob> uploadQueue;
extern std::vector<GLuint> deleteQueueVAO;
extern std::vector<GLuint> deleteQueueVBO;

extern std::mutex inventoryMutex;
extern std::atomic<uint32_t> inventoryVersion;

extern std::atomic<int> LoadRadius;
extern std::atomic<int> MeshRadius;
extern std::atomic<int> RenderRadius;
extern std::atomic<int> UnloadRadius;

extern std::atomic<float> settingFOV;
extern std::atomic<float> currentFOV;
extern std::atomic<float> settingSensitivity;
extern std::atomic<bool> settingInvertY;
extern std::atomic<int> settingRenderDistance;
extern std::atomic<int> settingGraphicsQuality;
extern std::atomic<bool> settingShadows;
extern std::atomic<bool> settingClouds;
extern std::atomic<bool> settingFog;
extern std::atomic<float> settingBrightness;

extern std::atomic<float> cameraEyeY;
extern std::atomic<float> bobTime;
extern std::atomic<float> bobAmount;
extern std::atomic<bool> settingViewBobbing;
extern std::atomic<bool> settingDebugWorld;
extern std::atomic<float> cameraLookX;
extern std::atomic<float> cameraLookY;

extern std::atomic<float> playerX;
extern std::atomic<float> playerY;
extern std::atomic<float> playerZ;

extern std::atomic<float> timeOfDay;

extern std::atomic<int> worldSeed;

extern std::atomic<bool> inputBreak;
extern std::atomic<bool> inputPlace;
extern std::atomic<float> miningProgress;
extern std::atomic<int> miningTargetX;
extern std::atomic<int> miningTargetY;
extern std::atomic<int> miningTargetZ;
extern std::atomic<float> placeCooldown;
extern std::atomic<float> breakCooldown;
extern const float INTERACTION_REACH;

enum class LocomotionState {
    STANDING,
    WALKING,
    SPRINTING,
    CROUCHING,
    JUMPING,
    FALLING,
    SWIMMING,
    CRAWLING,
    CLIMBING,
    FLYING,
    DEAD
};
extern LocomotionState playerState;
extern std::atomic<bool> isSubmerged;
extern std::atomic<bool> isInWater;
extern std::atomic<bool> isClimbing;
extern std::atomic<bool> isCrawlingState;

extern std::atomic<float> playerHealth;
extern std::atomic<float> playerMaxHealth;
extern std::atomic<bool> isDead;
extern std::atomic<float> damageCooldown;
extern std::atomic<float> fallDistance;
extern std::atomic<float> highestY;
extern std::atomic<float> hurtTime;
