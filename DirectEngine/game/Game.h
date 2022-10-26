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
	MeshData* mesh;
};

class Game : public IGame
{
public:
	std::wstring name = {};
	bool showLog = true;
	bool stopLog = false;
	RingLog debugLog{};

	MemoryArena globalArena{ 1024 * 1024 };
	MemoryArena entityArena{ 1024 * 1024 };

	EngineInput input{ globalArena };

	Game(std::wstring name);
	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;

	MeshData* LoadMeshFromFile(EngineCore& engine, const std::string& filePath);

	EngineInput& GetInput() override;

	void Log(std::string message) override;
	void Warn(std::string message) override;
	void Error(std::string message) override;
};
