#include "Globals.h"

std::atomic<bool> isRunning(false);
std::atomic<bool> isSimulationActive(false);
bool windowChanged = false;
std::mutex windowMutex;
std::condition_variable windowCV;
std::thread renderThread;
std::thread workerThread;
ANativeWindow* currentWindow = nullptr;
int width = 0;
int height = 0;

std::atomic<bool> isDebugFly(false);
std::atomic<bool> isInventoryOpen(false);
glm::vec3 playerVelocity(0.0f);
std::atomic<bool> isGrounded(false);
std::atomic<bool> isCrouching(false);
std::atomic<bool> isSprinting(false);
std::atomic<float> inputMoveX(0.0f);
std::atomic<float> inputMoveY(0.0f);
std::atomic<bool> inputJump(false);

std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;
std::mutex worldMutex;

std::mutex queueMutex;
std::condition_variable workerCV;
std::vector<ChunkPos> generateQueue;
std::vector<ChunkPos> meshQueue;

std::mutex uploadMutex;
std::vector<GPUUploadJob> uploadQueue;
std::vector<GLuint> deleteQueueVAO;
std::vector<GLuint> deleteQueueVBO;

std::mutex inventoryMutex;
std::atomic<uint32_t> inventoryVersion(1);

std::atomic<float> cameraEyeY(1.8f);
std::atomic<float> bobTime(0.0f);
std::atomic<float> bobAmount(0.0f);
std::atomic<bool> settingViewBobbing(true);
std::atomic<bool> settingDebugWorld(false);
std::atomic<float> cameraLookX(0.0f);
std::atomic<float> cameraLookY(0.0f);

std::atomic<float> playerX(0.0f);
std::atomic<float> playerY(16.0f);
std::atomic<float> playerZ(0.0f);

std::atomic<float> timeOfDay(0.0f);

std::atomic<int> LoadRadius(8);
std::atomic<int> MeshRadius(6);
std::atomic<int> RenderRadius(5);
std::atomic<int> UnloadRadius(10);

std::atomic<float> settingFOV(75.0f);
std::atomic<float> currentFOV(75.0f);
std::atomic<float> settingSensitivity(1.0f);
std::atomic<bool> settingInvertY(false);
std::atomic<int> settingRenderDistance(5);
std::atomic<int> settingGraphicsQuality(1);
std::atomic<bool> settingShadows(true);
std::atomic<bool> settingClouds(true);
std::atomic<bool> settingFog(true);
std::atomic<float> settingBrightness(1.0f);

std::atomic<int> worldSeed(123456);

std::atomic<bool> inputBreak(false);
std::atomic<bool> inputPlace(false);
std::atomic<float> miningProgress(0.0f);
std::atomic<int> miningTargetX(0);
std::atomic<int> miningTargetY(0);
std::atomic<int> miningTargetZ(0);
std::atomic<float> placeCooldown(0.0f);
std::atomic<float> breakCooldown(0.0f);
const float INTERACTION_REACH = 5.0f;

LocomotionState playerState = LocomotionState::STANDING;
std::atomic<bool> isSubmerged(false);
std::atomic<bool> isInWater(false);
std::atomic<bool> isClimbing(false);
std::atomic<bool> isCrawlingState(false);

std::atomic<float> playerHealth(20.0f);
std::atomic<float> playerMaxHealth(20.0f);
std::atomic<bool> isDead(false);
std::atomic<float> damageCooldown(0.0f);
std::atomic<float> fallDistance(0.0f);
std::atomic<float> highestY(0.0f);
std::atomic<float> hurtTime(0.0f);
