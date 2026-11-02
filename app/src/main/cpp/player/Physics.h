#pragma once
#include "../core/Globals.h"

bool checkCollision(glm::vec3 pos, glm::vec3 size);
void tickPhysics(float dt);

uint8_t getBlockAt(glm::vec3 pos);

void applyDamage(float amount, const char* source);
