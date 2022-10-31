#pragma once

#include "IGame.h"
#include "../core/EngineCore.h"
#include "imgui.h"
#include "ImGuiProfilerRenderer.h"

#include <string>
#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

const int LOG_SIZE = 1024;

struct WindowUpdate
{
	bool updateCursor;
	bool cursorVisible;
	bool cursorClipped;
};

struct LogMessage
{
	ImColor color;
	std::string message;
};

class RingLog
{
public:
	void DrawText();
	void AppendToLog(std::string message, ImU32 color);

private:
	LogMessage messages[LOG_SIZE] = {};
	int debugLogIndex = 0;
	int debugLogCount = 0;
};

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

class Entity
{
public:
	XMFLOAT3 position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	XMFLOAT3 scale{ 1.f, 1.f, 1.f };
	size_t dataIndex;
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

	Game();
	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);

	size_t LoadMeshFromFile(EngineCore& engine, const std::string& filePath);
	Entity* CreateEntity(EngineCore& engine, size_t meshIndex);
	void UpdateCursorState();

	EngineInput& GetInput() override;

	void Log(std::string message) override;
	void Warn(std::string message) override;
	void Error(std::string message) override;
};
