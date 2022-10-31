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

	virtual void Log(const std::string& message) = 0;
	virtual void Warn(const std::string& message) = 0;
	virtual void Error(const std::string& message) = 0;
};