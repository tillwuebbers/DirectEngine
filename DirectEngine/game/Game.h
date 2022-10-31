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
	size_t dataIndex;

	XMVECTOR color{ 1.f, 0.f, 1.f };

	bool isSpinning = false;
};

class Camera
{
public:
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

	WindowUpdate windowUpdateData{};
	std::mutex windowUdpateDataMutex;

	MemoryArena globalArena{};
	MemoryArena entityArena{};

	Camera camera{};
	EngineInput input{ globalArena };

	float playerPitch = 0.f;
	float playerYaw = 0.f;

	PuzzleSolver* solver;
	Entity* puzzleEntities[MAX_PIECE_COUNT];

	MemoryArena puzzleArena{};
	SlidingPuzzle displayedPuzzle;
	std::coroutine_handle<> displayCoroutine = nullptr;

	Game();
	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);

	size_t CreateMeshFromFile(EngineCore& engine, const std::string& filePath, const std::string& materialPath);
	Entity* CreateEntity(EngineCore& engine, size_t meshIndex);
	void UpdateCursorState();

	EngineInput& GetInput() override;

	void Log(const std::string& message) override;
	void Warn(const std::string& message) override;
	void Error(const std::string& message) override;
};
