#pragma once

#include "Entity.h"
#include "IGame.h"
#include "Puzzle.h"
#include "Log.h"
#include "Mesh.h"
#include "../core/EngineCore.h"

#include "imgui.h"
#include "ImGuiProfilerRenderer.h"

#include <string>
#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

const float BLOCK_DISPLAY_GAP = 0.1f;
const float SOLUTION_PLAYBACK_SPEED = 0.5f;

enum CollisionLayers : uint64_t
{
	None = 0,
	ClickTest = 1,
	Floor = 2,
	Dead = 4,
};

struct WindowUpdate
{
	bool updateCursor;
	bool cursorVisible;
	bool cursorClipped;
};

struct DirectionalLight
{
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	float shadowmapCameraSize;
};

struct CollisionResult
{
	Entity* entity;
	float distance;
};

struct Camera
{
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	float fovX = 0.f;
	float fovY = 45.f;
	float nearClip = .1f;
	float farClip = 1000.f;
};

class Game : public IGame
{
public:
	bool showLog = true;
	bool stopLog = false;
	RingLog debugLog{};

	bool showProfiler = true;
	bool pauseProfiler = false;
	ImGuiUtils::ProfilersWindow profilerWindow{};
	FrameDebugData lastFrames[256] = {};
	size_t lastDebugFrameIndex = 0;

	bool showDemoWindow = false;
	bool showEscMenu = false;
	bool showDebugUI = true;
	bool showDebugImage = true;
	bool showPostProcessImage = true;
	bool showMovementWindow = true;
	bool scrollLog = true;
	bool noclip = false;

	WindowUpdate windowUpdateData{};
	std::mutex windowUdpateDataMutex;

	MemoryArena globalArena{};
	MemoryArena entityArena{};
	MemoryArena dynamicGameArena{};
	MemoryArena vertexUploadArena{};
	MemoryArena indexUploadArena{};

	DirectionalLight light{};
	Camera camera{};
	EngineInput input{ globalArena };
	Texture diffuseTexture{};
	Texture memeTexture{};
	Texture kaijuTexture{};

	Entity* enemies[MAX_ENENMY_COUNT];
	XMVECTOR enemyVelocities[MAX_ENENMY_COUNT]{};
	bool enemyActive[MAX_ENENMY_COUNT]{};

	float playerPitch = 0.f;
	float playerYaw = 0.f;
	XMVECTOR playerVelocity = { 0.f, 0.f, 0.f };

	float playerHeight = 1.5f;
	float playerAcceleration = 125.f;
	float playerFriction = 125.f;
	float playerMaxSpeed = 20.f;
	float playerJumpStrength = 15.f;
	float playerGravity = 35.f;
	float inputDeadzone = 0.05f;

	float enemyAcceleration = 200.f;
	float enemyMaxSpeed = 15.f;
	float enemySpawnRate = 5.f;

	float jumpBufferDuration = 1.f;
	float lastJumpPressTime = -1000.f;
	float lastEnemySpawn = 0.f;

	XMVECTOR clearColor = { .1f, .2f, .4f, 1.f };
	float contrast = 1.;
	float brightness = 0.;
	float saturation = 1.;
	float fog = 0.;

	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);

	CollisionResult CollideWithWorld(const XMVECTOR rayOrigin, const XMVECTOR rayDirection, uint64_t matchingLayers);

	Entity* CreateEntity(EngineCore& engine, size_t drawCallIndex, D3D12_VERTEX_BUFFER_VIEW& meshView, MemoryArena* arena = nullptr);
	Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, MemoryArena* arena = nullptr);
	void UpdateCursorState();

	float* GetClearColor() override;
	EngineInput& GetInput() override;

	void Log(const std::string& message) override;
	void Warn(const std::string& message) override;
	void Error(const std::string& message) override;
};
