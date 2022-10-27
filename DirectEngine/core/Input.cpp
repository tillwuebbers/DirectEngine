#include "Input.h"

void KeyBuffer::Reset()
{
	for (int i = 0; i < MAX_KEY_CODE; i++)
	{
		keys[i] = false;
	}
}

EngineInput::EngineInput(MemoryArena& arena)
{
	currentPressedKeys = NewObject(arena, KeyBuffer);
	currentReleasedKeys = NewObject(arena, KeyBuffer);
	keysDown = NewObject(arena, KeyBuffer);
}

void EngineInput::NextFrame()
{
	currentPressedKeys->Reset();
	currentReleasedKeys->Reset();
}

bool EngineInput::KeyJustPressed(unsigned int keyCode)
{
	return currentPressedKeys->keys[keyCode];
}

bool EngineInput::KeyDown(unsigned int keyCode)
{
	return keysDown->keys[keyCode];
}

bool EngineInput::KeyJustReleased(unsigned int keyCode)
{
	return currentReleasedKeys->keys[keyCode];
}