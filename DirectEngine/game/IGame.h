#pragma once

#include "../core/Input.h"
#include <string>

class EngineCore;

class IGame
{
public:
	virtual void StartGame(EngineCore& engine) = 0;
	virtual void UpdateGame(EngineCore& engine) = 0;

	virtual EngineInput& GetInput() = 0;

	virtual void Log(std::string message) = 0;
	virtual void Warn(std::string message) = 0;
	virtual void Error(std::string message) = 0;
};