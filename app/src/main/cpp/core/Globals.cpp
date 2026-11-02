#include "Globals.h"



std::atomic<bool> isRunning(false);
bool windowChanged = false;
std::mutex windowMutex;
std::condition_variable windowCV;
std::thread renderThread;
std::thread workerThread;
ANativeWindow* currentWindow = nullptr;
int width = 0;
int height = 0;



bool isDebugFly = false;
glm::vec3 playerVelocity(0.0f);
bool isGrounded = false;
bool isCrouching = false;
bool isSprinting = false;
float inputMoveX = 0.0f;
float inputMoveY = 0.0f;
bool inputJump = false;



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

float cameraEyeY = 1.8f;
float bobTime = 0.0f;
float bobAmount = 0.0f;
bool settingViewBobbing = true;
bool settingDebugWorld = false;
float cameraLookX = 0.0f;
float cameraLookY = 0.0f;

float playerX = 0.0f;
float playerY = 16.0f;
float playerZ = 0.0f;

float timeOfDay = 0.0f;



int LoadRadius = 8;
int MeshRadius = 6;
int RenderRadius = 5;
int UnloadRadius = 10;

float settingFOV = 75.0f;
float currentFOV = 75.0f;
float settingSensitivity = 1.0f;
bool settingInvertY = false;
int settingRenderDistance = 5;
int settingGraphicsQuality = 1;
bool settingShadows = true;
bool settingClouds = true;
bool settingFog = true;
float settingBrightness = 1.0f;

int worldSeed = 123456;

bool inputBreak = false;
bool inputPlace = false;
float miningProgress = 0.0f;
int miningTargetX = 0;
int miningTargetY = 0;
int miningTargetZ = 0;
float placeCooldown = 0.0f;
float breakCooldown = 0.0f;
const float INTERACTION_REACH = 5.0f;

LocomotionState playerState = LocomotionState::STANDING;
bool isSubmerged = false;
bool isInWater = false;
bool isClimbing = false;
bool isCrawlingState = false;

float playerHealth = 20.0f;
float playerMaxHealth = 20.0f;
bool isDead = false;
float damageCooldown = 0.0f;
float fallDistance = 0.0f;
float highestY = 0.0f;
float hurtTime = 0.0f;
