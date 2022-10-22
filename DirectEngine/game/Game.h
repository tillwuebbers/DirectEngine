#pragma once

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

class Game
{
public:
	std::wstring name = {};
	bool showLog = true;
	bool stopLog = false;
	RingLog debugLog{};

	Game(std::wstring name);
	void StartGame();
	void UpdateGame();

	void Log(std::string message);
	void Warn(std::string message);
	void Error(std::string message);
};