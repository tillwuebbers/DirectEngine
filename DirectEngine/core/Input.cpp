#include "Input.h"

void KeyBuffer::Set(unsigned int keyCode)
{
	if (pressedKeyCount >= MAX_KEYS_PRESSED || Contains(keyCode)) return;

	pressedKeys[pressedKeyCount] = keyCode;
	pressedKeyCount++;
}

bool KeyBuffer::Contains(unsigned int keyCode)
{
	for (int i = 0; i < pressedKeyCount; i++)
	{
		if (pressedKeys[i] == keyCode) return true;
	}
	return false;
}

void KeyBuffer::Reset()
{
	pressedKeyCount = 0;
}

EngineInput::EngineInput(MemoryArena& arena)
{
	currentPressedKeys = NewObject(arena, KeyBuffer);
	previousPressedKeys = NewObject(arena, KeyBuffer);
}

void EngineInput::NextFrame()
{
	KeyBuffer* temp = previousPressedKeys;
	previousPressedKeys = currentPressedKeys;
	currentPressedKeys = temp;
	currentPressedKeys->Reset();
}

bool EngineInput::KeyJustPressed(unsigned int keyCode)
{
	return KeyDown(keyCode) && !KeyDownLastFrame(keyCode);
}

bool EngineInput::KeyDown(unsigned int keyCode)
{
	return currentPressedKeys->Contains(keyCode);
}

bool EngineInput::KeyDownLastFrame(unsigned int keyCode)
{
	return previousPressedKeys->Contains(keyCode);
}

bool EngineInput::KeyJustReleased(unsigned int keyCode)
{
	return !KeyDown(keyCode) && KeyDownLastFrame(keyCode);
}