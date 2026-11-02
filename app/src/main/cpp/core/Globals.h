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

extern bool isDebugFly;
extern glm::vec3 playerVelocity;
extern bool isGrounded;
extern bool isCrouching;
extern bool isSprinting;
extern float inputMoveX;
extern float inputMoveY;
extern bool inputJump;

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
    uint8_t data[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
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

extern int LoadRadius;
extern int MeshRadius;
extern int RenderRadius;
extern int UnloadRadius;

extern float settingFOV;
extern float currentFOV;
extern float settingSensitivity;
extern bool settingInvertY;
extern int settingRenderDistance;
extern int settingGraphicsQuality;
extern bool settingShadows;
extern bool settingClouds;
extern bool settingFog;
extern float settingBrightness;

extern float cameraEyeY;
extern float bobTime;
extern float bobAmount;
extern bool settingViewBobbing;
extern bool settingDebugWorld;
extern float cameraLookX;
extern float cameraLookY;

extern float playerX;
extern float playerY;
extern float playerZ;

extern float timeOfDay;



extern int worldSeed;

extern bool inputBreak;
extern bool inputPlace;
extern float miningProgress;
extern int miningTargetX;
extern int miningTargetY;
extern int miningTargetZ;
extern float placeCooldown;
extern float breakCooldown;
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
extern bool isSubmerged;
extern bool isInWater;
extern bool isClimbing;
extern bool isCrawlingState;

extern float playerHealth;
extern float playerMaxHealth;
extern bool isDead;
extern float damageCooldown;
extern float fallDistance;
extern float highestY;
extern float hurtTime;
