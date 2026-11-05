#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <math.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/Logger.h"
#include "shaders/Shaders.h"

#include "core/Globals.h"
#include "rendering/Renderer.h"
#include "world/World.h"
#include "player/Physics.h"
#include "gameplay/Raycast.h"
#include "world/ChunkManager.h"

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOGE("Shader compilation failed: %s", infoLog);
    }
    return shader;
}

uint32_t atlasPixelsARGB[256 * 256];
bool atlasGenerated = false;
extern uint32_t* externalAtlasPixels;
extern bool useExternalAtlas;
extern int externalAtlasWidth;
extern int externalAtlasHeight;

void generateTextureAtlas(GLuint& texID) {
    LOGI("DIAGNOSTICS: generateTextureAtlas called");
    int width, height;
    uint8_t* glPixels = nullptr;

    if (!externalAtlasPixels) {
        LOGE("FATAL: externalAtlasPixels is NULL. Atlas was not loaded!");
        width = 2; height = 2;
        glPixels = new uint8_t[16]{
            255, 0, 255, 255,   0, 0, 0, 255,
            0, 0, 0, 255,       255, 0, 255, 255
        };
    } else {
        width = externalAtlasWidth;
        height = externalAtlasHeight;
        LOGI("DIAGNOSTICS: externalAtlasPixels is NON-NULL. width=%d, height=%d", width, height);
        glPixels = new uint8_t[width * height * 4];
        for(int i = 0; i < width * height; ++i) {
            uint32_t c = externalAtlasPixels[i];
            glPixels[i * 4 + 0] = (c >> 16) & 0xFF; // R
            glPixels[i * 4 + 1] = (c >> 8) & 0xFF;  // G
            glPixels[i * 4 + 2] = (c >> 0) & 0xFF;  // B
            glPixels[i * 4 + 3] = (c >> 24) & 0xFF; // A
        }
    }

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, glPixels);
    
    GLenum err = glGetError();
    LOGI("DIAGNOSTICS: glGetError after glTexImage2D = 0x%x", err);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    delete[] glPixels;

    if (externalAtlasPixels) {
        delete[] externalAtlasPixels;
        externalAtlasPixels = nullptr;
        LOGI("DIAGNOSTICS: externalAtlasPixels RAM buffer freed after GPU upload");
    }
}

void renderLoop() {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);
    
    EGLSurface surface = EGL_NO_SURFACE;

    bool resourcesLoaded = false;
    GLuint shaderProgram = 0;
    GLuint skyShaderProgram = 0;
    GLuint atlasTex = 0;
    GLint projLoc = -1;
    GLint viewLoc = -1;
    GLint modelLoc = -1;
    GLint sunDirLoc = -1;
    GLint sunColorLoc = -1;
    GLint horizonColorLoc = -1;
    GLint zenithColorLoc = -1;
    GLint cameraPosLoc = -1;
    GLint uUseFogLoc = -1;
    GLint uUseShadowsLoc = -1;
    GLint uUseCloudsLoc = -1;
    GLuint outlineShaderProgram = 0;
    GLint outlineModelLoc = -1;
    GLint outlineViewLoc = -1;
    GLint outlineProjLoc = -1;
    GLint outlineColorLoc = -1;
    GLuint outlineVAO = 0;
    GLuint outlineVBO = 0;
    GLint uBrightnessLoc = -1;
    GLint skyBrightnessLoc = -1;
    
    GLint skyInvProjViewLoc = -1;
    GLint skySunDirLoc = -1;
    GLint skyHorizonColorLoc = -1;
    GLint skyZenithColorLoc = -1;
    GLint skySunColorLoc = -1;
    GLint skyTimeLoc = -1;
    GLint skyCameraPosLoc = -1;
    
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (isRunning.load()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        if (isSimulationActive.load()) {
            tickPhysics(dt);
            tickInteraction(dt);
        }

        bool hasWindow = false;
        {
            std::unique_lock<std::mutex> lock(windowMutex);
            if (windowChanged) {
                if (surface != EGL_NO_SURFACE) {
                    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroySurface(display, surface);
                    surface = EGL_NO_SURFACE;
                }
                if (currentWindow) {
                    surface = eglCreateWindowSurface(display, config, currentWindow, nullptr);
                }
                windowChanged = false;
                windowCV.notify_all();
            }
            hasWindow = (surface != EGL_NO_SURFACE);
        }

        if (hasWindow) {
            eglMakeCurrent(display, surface, surface, context);
            
            if (!resourcesLoaded) {
                GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
                GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
                shaderProgram = glCreateProgram();
                glAttachShader(shaderProgram, vertexShader);
                glAttachShader(shaderProgram, fragmentShader);
                glLinkProgram(shaderProgram);
                glDeleteShader(vertexShader);
                glDeleteShader(fragmentShader);
                
                GLuint skyVS = compileShader(GL_VERTEX_SHADER, skyVertexShaderSource);
                GLuint skyFS = compileShader(GL_FRAGMENT_SHADER, skyFragmentShaderSource);
                skyShaderProgram = glCreateProgram();
                glAttachShader(skyShaderProgram, skyVS);
                glAttachShader(skyShaderProgram, skyFS);
                glLinkProgram(skyShaderProgram);
                glDeleteShader(skyVS);
                glDeleteShader(skyFS);

                GLuint outlineVS = compileShader(GL_VERTEX_SHADER, wireframeVertexShaderSource);
                GLuint outlineFS = compileShader(GL_FRAGMENT_SHADER, wireframeFragmentShaderSource);
                outlineShaderProgram = glCreateProgram();
                glAttachShader(outlineShaderProgram, outlineVS);
                glAttachShader(outlineShaderProgram, outlineFS);
                glLinkProgram(outlineShaderProgram);
                glDeleteShader(outlineVS);
                glDeleteShader(outlineFS);
                
                outlineModelLoc = glGetUniformLocation(outlineShaderProgram, "model");
                outlineViewLoc = glGetUniformLocation(outlineShaderProgram, "view");
                outlineProjLoc = glGetUniformLocation(outlineShaderProgram, "projection");
                outlineColorLoc = glGetUniformLocation(outlineShaderProgram, "u_color");
                
                glGenVertexArrays(1, &outlineVAO);
                glGenBuffers(1, &outlineVBO);
                glBindVertexArray(outlineVAO);
                glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
                float e = -0.005f;
                float s = 1.005f;
                float cubeLines[] = {
                    e,e,e, s,e,e,  s,e,e, s,e,s,  s,e,s, e,e,s,  e,e,s, e,e,e,
                    e,s,e, s,s,e,  s,s,e, s,s,s,  s,s,s, e,s,s,  e,s,s, e,s,e,
                    e,e,e, e,s,e,  s,e,e, s,s,e,  s,e,s, s,s,s,  e,e,s, e,s,s
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(cubeLines), cubeLines, GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);

                generateTextureAtlas(atlasTex);
                glUseProgram(shaderProgram);
                GLint atlasLoc = glGetUniformLocation(shaderProgram, "atlas");
                glUniform1i(atlasLoc, 0);
                
                projLoc = glGetUniformLocation(shaderProgram, "projection");
                viewLoc = glGetUniformLocation(shaderProgram, "view");
                modelLoc = glGetUniformLocation(shaderProgram, "model");
                sunDirLoc = glGetUniformLocation(shaderProgram, "sunDir");
                sunColorLoc = glGetUniformLocation(shaderProgram, "sunColor");
                horizonColorLoc = glGetUniformLocation(shaderProgram, "horizonColor");
                zenithColorLoc = glGetUniformLocation(shaderProgram, "zenithColor");
                cameraPosLoc = glGetUniformLocation(shaderProgram, "cameraPos");
                uUseFogLoc = glGetUniformLocation(shaderProgram, "u_useFog");
                uUseShadowsLoc = glGetUniformLocation(shaderProgram, "u_useShadows");
                uBrightnessLoc = glGetUniformLocation(shaderProgram, "u_brightness");
                
                skyInvProjViewLoc = glGetUniformLocation(skyShaderProgram, "invProjView");
                skySunDirLoc = glGetUniformLocation(skyShaderProgram, "sunDir");
                skyHorizonColorLoc = glGetUniformLocation(skyShaderProgram, "horizonColor");
                skyZenithColorLoc = glGetUniformLocation(skyShaderProgram, "zenithColor");
                skySunColorLoc = glGetUniformLocation(skyShaderProgram, "sunColor");
                skyTimeLoc = glGetUniformLocation(skyShaderProgram, "time");
                skyCameraPosLoc = glGetUniformLocation(skyShaderProgram, "cameraPos");
                uUseCloudsLoc = glGetUniformLocation(skyShaderProgram, "u_useClouds");
                skyBrightnessLoc = glGetUniformLocation(skyShaderProgram, "u_brightness");

                glEnable(GL_DEPTH_TEST);
                resourcesLoaded = true;
            }

            glViewport(0, 0, width, height);
            
            float tOfDay = timeOfDay.load() + 0.002f;
            timeOfDay.store(tOfDay);
            float sunH = sin(tOfDay);
            
            glm::vec3 zenithNight = glm::vec3(0.02f, 0.03f, 0.08f);
            glm::vec3 zenithDay = glm::vec3(0.15f, 0.4f, 0.9f);
            glm::vec3 zenithColor = glm::mix(zenithNight, zenithDay, glm::clamp(sunH + 0.3f, 0.0f, 1.0f));
            
            glm::vec3 horizonNight = glm::vec3(0.05f, 0.06f, 0.12f);
            glm::vec3 horizonSunrise = glm::vec3(0.9f, 0.4f, 0.2f);
            glm::vec3 horizonDay = glm::vec3(0.6f, 0.8f, 1.0f);
            
            glm::vec3 horizonColor;
            if (sunH < -0.2f) {
                horizonColor = horizonNight;
            } else if (sunH < 0.2f) {
                float t = (sunH + 0.2f) / 0.4f;
                if (t < 0.5f) horizonColor = glm::mix(horizonNight, horizonSunrise, t * 2.0f);
                else horizonColor = glm::mix(horizonSunrise, horizonDay, (t - 0.5f) * 2.0f);
            } else {
                horizonColor = horizonDay;
            }
            
            glm::vec3 sunColor = glm::mix(glm::vec3(1.0f, 0.5f, 0.2f), glm::vec3(1.0f, 1.0f, 1.0f), glm::clamp(sunH * 2.0f, 0.0f, 1.0f));
            glm::vec3 sunDir = glm::normalize(glm::vec3(cos(tOfDay), sin(tOfDay), 0.5f));
            
            if (sunH < -0.1f) {
                sunColor = glm::vec3(0.1f, 0.15f, 0.25f);
                sunDir = glm::normalize(glm::vec3(cos(tOfDay + 3.14159f), sin(tOfDay + 3.14159f), 0.5f));
            } else if (sunH < 0.1f) {
                float t = (sunH + 0.1f) / 0.2f;
                sunColor = glm::mix(glm::vec3(0.1f, 0.15f, 0.25f), sunColor, t);
            }
            
            glClearColor(horizonColor.r, horizonColor.g, horizonColor.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            float fov = currentFOV.load();
            glm::mat4 projection = glm::perspective(glm::radians(fov), (float)width / (float)height, 0.1f, 100.0f);
            
            float eyeY = cameraEyeY.load();
            float pX = playerX.load();
            float pY = playerY.load();
            float pZ = playerZ.load();
            glm::vec3 cameraPos = glm::vec3(pX, pY + eyeY, pZ);
            
            float cLookX = cameraLookX.load();
            float cLookY = cameraLookY.load();
            glm::vec3 front;
            front.x = cos(glm::radians(cLookX)) * cos(glm::radians(cLookY));
            front.y = sin(glm::radians(cLookY));
            front.z = sin(glm::radians(cLookX)) * cos(glm::radians(cLookY));
            glm::vec3 cameraFront = glm::normalize(front);
            
            glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));

            if (settingViewBobbing.load()) {
                float bTime = bobTime.load();
                float bAmt = bobAmount.load();
                float bobOffset = sin(bTime) * 0.05f * bAmt;
                float bobSway = cos(bTime * 0.5f) * 0.05f * bAmt;
                cameraPos.y += bobOffset;
                cameraPos += cameraRight * bobSway;
            }
            
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            glm::mat4 invProjView = glm::inverse(projection * view);
            
            // Draw Sky Quad
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glUseProgram(skyShaderProgram);
            if (skyInvProjViewLoc != -1) glUniformMatrix4fv(skyInvProjViewLoc, 1, GL_FALSE, glm::value_ptr(invProjView));
            if (skySunDirLoc != -1) glUniform3fv(skySunDirLoc, 1, glm::value_ptr(sunDir));
            if (skyHorizonColorLoc != -1) glUniform3fv(skyHorizonColorLoc, 1, glm::value_ptr(horizonColor));
            if (skyZenithColorLoc != -1) glUniform3fv(skyZenithColorLoc, 1, glm::value_ptr(zenithColor));
            if (skySunColorLoc != -1) glUniform3fv(skySunColorLoc, 1, glm::value_ptr(sunColor));
            if (skyTimeLoc != -1) glUniform1f(skyTimeLoc, tOfDay * 10.0f);
            if (skyCameraPosLoc != -1) glUniform3fv(skyCameraPosLoc, 1, glm::value_ptr(cameraPos));
            glUniform1i(uUseCloudsLoc, settingClouds.load() ? 1 : 0);
            glUniform1f(skyBrightnessLoc, settingBrightness.load());
            
            glDrawArrays(GL_TRIANGLES, 0, 3);
            
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            
            // Draw Terrain
            glUseProgram(shaderProgram);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, atlasTex);

            glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
            if (sunDirLoc != -1) glUniform3fv(sunDirLoc, 1, glm::value_ptr(sunDir));
            if (sunColorLoc != -1) glUniform3fv(sunColorLoc, 1, glm::value_ptr(sunColor));
            if (horizonColorLoc != -1) glUniform3fv(horizonColorLoc, 1, glm::value_ptr(horizonColor));
            if (zenithColorLoc != -1) glUniform3fv(zenithColorLoc, 1, glm::value_ptr(zenithColor));
            if (cameraPosLoc != -1) glUniform3fv(cameraPosLoc, 1, glm::value_ptr(cameraPos));
            glUniform1i(uUseFogLoc, settingFog.load() ? 1 : 0);
            glUniform1i(uUseShadowsLoc, settingShadows.load() ? 1 : 0);
            glUniform1f(uBrightnessLoc, settingBrightness.load());

            std::vector<GPUUploadJob> localUploads;
            std::vector<GLuint> vaosToDelete;
            std::vector<GLuint> vbosToDelete;
            {
                std::lock_guard<std::mutex> upLock(uploadMutex);
                int uploadsThisFrame = 0;
                while (!uploadQueue.empty() && uploadsThisFrame < 4) {
                    localUploads.push_back(std::move(uploadQueue.front()));
                    uploadQueue.erase(uploadQueue.begin());
                    uploadsThisFrame++;
                }
                
                vaosToDelete = std::move(deleteQueueVAO);
                vbosToDelete = std::move(deleteQueueVBO);
                deleteQueueVAO.clear();
                deleteQueueVBO.clear();
            }

            for (auto& job : localUploads) {
                std::lock_guard<std::mutex> worldLock(worldMutex);
                auto it = chunks.find(job.pos);
                if (it != chunks.end()) {
                    if (it->second.VAO != 0) {
                        vaosToDelete.push_back(it->second.VAO);
                        vbosToDelete.push_back(it->second.VBO);
                    }
                    glGenVertexArrays(1, &it->second.VAO);
                    glGenBuffers(1, &it->second.VBO);
                    
                    glBindVertexArray(it->second.VAO);
                    glBindBuffer(GL_ARRAY_BUFFER, it->second.VBO);
                    glBufferData(GL_ARRAY_BUFFER, job.meshData.size() * sizeof(float), job.meshData.data(), GL_STATIC_DRAW);
                    
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
                    glEnableVertexAttribArray(0);

                    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float)));
                    glEnableVertexAttribArray(1);

                    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(5 * sizeof(float)));
                    glEnableVertexAttribArray(2);

                    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(6 * sizeof(float)));
                    glEnableVertexAttribArray(3);

                    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(9 * sizeof(float)));
                    glEnableVertexAttribArray(4);
                    
                    it->second.vertexCount = job.meshData.size() / 10;
                    it->second.state = ChunkState::GPUResident;
                }
            }

            if (!vaosToDelete.empty()) {
                glDeleteVertexArrays(vaosToDelete.size(), vaosToDelete.data());
            }
            if (!vbosToDelete.empty()) {
                glDeleteBuffers(vbosToDelete.size(), vbosToDelete.data());
            }

            glm::mat4 vp = projection * view;
            struct Plane { glm::vec3 normal; float distance; };
            Plane planes[6];
            planes[0] = {glm::vec3(vp[0][3]+vp[0][0], vp[1][3]+vp[1][0], vp[2][3]+vp[2][0]), vp[3][3]+vp[3][0]}; // Left
            planes[1] = {glm::vec3(vp[0][3]-vp[0][0], vp[1][3]-vp[1][0], vp[2][3]-vp[2][0]), vp[3][3]-vp[3][0]}; // Right
            planes[2] = {glm::vec3(vp[0][3]+vp[0][1], vp[1][3]+vp[1][1], vp[2][3]+vp[2][1]), vp[3][3]+vp[3][1]}; // Bottom
            planes[3] = {glm::vec3(vp[0][3]-vp[0][1], vp[1][3]-vp[1][1], vp[2][3]-vp[2][1]), vp[3][3]-vp[3][1]}; // Top
            planes[4] = {glm::vec3(vp[0][3]+vp[0][2], vp[1][3]+vp[1][2], vp[2][3]+vp[2][2]), vp[3][3]+vp[3][2]}; // Near
            planes[5] = {glm::vec3(vp[0][3]-vp[0][2], vp[1][3]-vp[1][2], vp[2][3]-vp[2][2]), vp[3][3]-vp[3][2]}; // Far
            for(int i=0; i<6; i++) {
                float length = glm::length(planes[i].normal);
                planes[i].normal /= length;
                planes[i].distance /= length;
            }

            struct RenderChunk { GLuint VAO; int vertexCount; glm::vec3 pos; };
            std::vector<RenderChunk> renderChunks;
            {
                std::lock_guard<std::mutex> lock(worldMutex);
                int rRad = RenderRadius.load();
                for (const auto& pair : chunks) {
                    if (pair.second.state == ChunkState::GPUResident && pair.second.vertexCount > 0) {
                        float dx = (pair.first.x * CHUNK_SIZE + 8) - pX;
                        float dz = (pair.first.z * CHUNK_SIZE + 8) - pZ;
                        if (dx*dx + dz*dz <= rRad*rRad*CHUNK_SIZE*CHUNK_SIZE) {
                            glm::vec3 min(pair.first.x * CHUNK_SIZE - 0.5f, -0.5f, pair.first.z * CHUNK_SIZE - 0.5f);
                            glm::vec3 max(pair.first.x * CHUNK_SIZE + CHUNK_SIZE - 0.5f, CHUNK_HEIGHT - 0.5f, pair.first.z * CHUNK_SIZE + CHUNK_SIZE - 0.5f);
                            
                            bool inside = true;
                            for(int i=0; i<6; i++) {
                                glm::vec3 positive(
                                    planes[i].normal.x > 0 ? max.x : min.x,
                                    planes[i].normal.y > 0 ? max.y : min.y,
                                    planes[i].normal.z > 0 ? max.z : min.z
                                );
                                if (glm::dot(planes[i].normal, positive) + planes[i].distance < 0) {
                                    inside = false;
                                    break;
                                }
                            }
                            if (inside) {
                                renderChunks.push_back({pair.second.VAO, pair.second.vertexCount, glm::vec3(pair.first.x * CHUNK_SIZE, 0, pair.first.z * CHUNK_SIZE)});
                            }
                        }
                    }
                }
            }
            
            for (const auto& rc : renderChunks) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, rc.pos);
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glBindVertexArray(rc.VAO);
                glDrawArrays(GL_TRIANGLES, 0, rc.vertexCount);
            }

            RaycastHit hit = getTargetBlock();
            if (hit.hasHit) {
                glUseProgram(outlineShaderProgram);
                glUniformMatrix4fv(outlineProjLoc, 1, GL_FALSE, glm::value_ptr(projection));
                glUniformMatrix4fv(outlineViewLoc, 1, GL_FALSE, glm::value_ptr(view));
                glBindVertexArray(outlineVAO);
                
                glUniform4f(outlineColorLoc, 0.0f, 0.0f, 0.0f, 0.8f);
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(hit.bx, hit.by, hit.bz));
                glUniformMatrix4fv(outlineModelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glDrawArrays(GL_LINES, 0, 24);
                
                if (inputPlace.load() && placeCooldown.load() > 0.0f) {
                } else if (!(hit.px == hit.bx && hit.py == hit.by && hit.pz == hit.bz)) {
                    glUniform4f(outlineColorLoc, 1.0f, 1.0f, 1.0f, 0.5f);
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(hit.px, hit.py, hit.pz));
                    glUniformMatrix4fv(outlineModelLoc, 1, GL_FALSE, glm::value_ptr(model));
                    glDrawArrays(GL_LINES, 0, 24);
                }

                float mProg = miningProgress.load();
                if (mProg > 0.0f && mProg < 1.0f) {
                    glDisable(GL_DEPTH_TEST);
                    glUniform4f(outlineColorLoc, 1.0f, 0.2f, 0.2f, 0.9f);
                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(hit.bx + 0.5f, hit.by + 0.5f, hit.bz + 0.5f));
                    model = glm::scale(model, glm::vec3(1.0f - mProg));
                    model = glm::translate(model, glm::vec3(-0.5f, -0.5f, -0.5f));
                    glUniformMatrix4fv(outlineModelLoc, 1, GL_FALSE, glm::value_ptr(model));
                    glDrawArrays(GL_LINES, 0, 24);
                    glEnable(GL_DEPTH_TEST);
                }
            }

            eglSwapBuffers(display, surface);
        } else {
            usleep(16000);
        }
    }
    
    if (surface != EGL_NO_SURFACE) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display, surface);
    }
    eglDestroyContext(display, context);
    eglTerminate(display);
}
