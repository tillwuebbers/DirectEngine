#pragma once

#include "../core/Input.h"
#include <string>
#include <mutex>

class EngineCore;

struct WindowUpdate
{
	bool updateCursor;
	bool cursorVisible;
	bool cursorClipped;
};

class IGame
{
public:
	virtual void StartGame(EngineCore& engine) = 0;
	virtual void UpdateGame(EngineCore& engine) = 0;
	virtual void BeforeMainRender(EngineCore& engine) = 0;

	virtual float* GetClearColor() = 0;
	virtual EngineInput& GetInput() = 0;
	virtual std::mutex& GetWindowUdpateDataMutex() = 0;
	virtual WindowUpdate& GetWindowUpdateData() = 0;
};