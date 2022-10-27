#pragma once

#include "IGame.h"
#include "../core/EngineCore.h"
#include "../imgui/imgui.h"

#include <string>
#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

const int LOG_SIZE = 1024;

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
	XMFLOAT4 rotation{ 0.f, 0.f, 0.f, 1.f };
	XMFLOAT3 scale{ 1.f, 1.f, 1.f };
	size_t meshIndex;
};

class Camera
{
public:
	XMFLOAT3 position{ 0.f, 0.f, 0.f };
	XMFLOAT4 rotation{ 0.f, 0.f, 0.f, 1.f };
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

	MemoryArena globalArena{};
	MemoryArena entityArena{};

	Camera camera{};
	EngineInput input{ globalArena };

	Game();
	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;

	size_t LoadMeshFromFile(EngineCore& engine, const std::string& filePath);

	EngineInput& GetInput() override;

	void Log(std::string message) override;
	void Warn(std::string message) override;
	void Error(std::string message) override;
};
