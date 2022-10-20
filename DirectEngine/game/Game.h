#pragma once

#include "../imgui/imgui.h"

#include <string>
#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

struct LogMessage
{
	ImColor color;
	std::string message;
};

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

class Game
{
public:
	std::wstring name;
	std::vector<LogMessage> debugLog;
	bool showLog;

	Game(std::wstring name);
	void StartGame();
	void UpdateGame();

	void Log(const char* message);
	void Log(std::string message);
	void Warn(const char* message);
	void Warn(std::string message);
	void Error(const char* message);
	void Error(std::string message);
};