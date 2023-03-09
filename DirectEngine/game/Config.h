#pragma once

#include "../core/Memory.h"

#define CONFIG_PATH "config/config.conf"

#define CONFIG_VERSION 1
struct ConfigFile
{
	uint32_t version = CONFIG_VERSION;
	uint32_t movementSettingsOffset = 0;
};

#define MOVEMENT_SETTINGS_VERSION 1
struct MovementSettings
{
	uint32_t version = MOVEMENT_SETTINGS_VERSION;
	float playerAcceleration = 125.f;
	float playerFriction = 125.f;
	float playerMaxSpeed = 20.f;
	float playerJumpStrength = 15.f;
	float playerGravity = 35.f;
	bool autojump = true;
};

bool LoadConfig(MemoryArena& arena, const char* path);
bool SaveConfig(MemoryArena& arena, const char* path);

template <typename T>
T* LoadConfigEntry(MemoryArena& arena, uint32_t offset, uint32_t expectedVersion)
{
	T* result = reinterpret_cast<T*>(arena.base + offset);
	if (result->version != expectedVersion) return nullptr;
	return result;
}
