#pragma once

#include "IGame.h"
#include "Puzzle.h"
#include "Log.h"
#include "Mesh.h"
#include "../core/EngineCore.h"
#include "../core/Coroutine.h"

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
	None,
	ClickTest,
};

struct WindowUpdate
{
	bool updateCursor;
	bool cursorVisible;
	bool cursorClipped;
};

class Entity
{
public:
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	XMVECTOR scale{ 1.f, 1.f, 1.f };

	EngineCore* engine;
	size_t materialIndex;
	size_t dataIndex;

	bool isSpinning = false;
	uint64_t collisionLayers = 0;

	EntityData* GetData();
	EntityConstantBuffer* GetBuffer();
	XMVECTOR LocalToWorld(XMVECTOR localPosition);
	XMVECTOR WorldToLocal(XMVECTOR worldPosition);
};

struct DirectionalLight
{
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	float shadowmapCameraSize;
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
	bool scrollLog = true;

	WindowUpdate windowUpdateData{};
	std::mutex windowUdpateDataMutex;

	MemoryArena globalArena{};
	MemoryArena entityArena{};
	MemoryArena vertexUploadArena{};
	MemoryArena indexUploadArena{};

	DirectionalLight light{};
	Camera camera{};
	EngineInput input{ globalArena };
	Texture diffuseTexture{};
	Texture memeTexture{};
	Texture kaijuTexture{};

	float playerPitch = 0.f;
	float playerYaw = 0.f;

	PuzzleSolver* solver;
	Entity* puzzleEntities[MAX_PIECE_COUNT];
	Entity* testCube;
	Entity* lightDisplayEntity;
	Entity* graphDisplayEntities[1024];

	MemoryArena puzzleArena{};
	SlidingPuzzle displayedPuzzle;
	std::coroutine_handle<> displayCoroutine = nullptr;

	Game();
	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);

	Entity* CreateEntity(EngineCore& engine, size_t drawCallIndex, D3D12_VERTEX_BUFFER_VIEW& meshView);
	void UpdateCursorState();

	EngineInput& GetInput() override;

	void Log(const std::string& message) override;
	void Warn(const std::string& message) override;
	void Error(const std::string& message) override;
};
