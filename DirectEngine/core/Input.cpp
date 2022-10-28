#include "Input.h"
#include "vkcodes.h"

const unsigned int modifiers[] = { VK_SHIFT, VK_CONTROL, VK_MENU };

bool MatchModifiers(KeyBuffer* buffer, unsigned int modifier1, unsigned int modifier2)
{
	for (unsigned int mod : modifiers)
	{
		if (!((modifier1 == mod || modifier2 == mod) == buffer->keys[mod])) return false;
	}
	return true;
}

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
	lastKeysDown = NewObject(arena, KeyBuffer);
}

void EngineInput::NextFrame()
{
	currentPressedKeys->Reset();
	currentReleasedKeys->Reset();
	std::copy(std::begin(keysDown->keys), std::end(keysDown->keys), std::begin(lastKeysDown->keys));
}

bool EngineInput::KeyJustPressed(unsigned int keyCode, unsigned int modifier1, unsigned int modifier2)
{
	return currentPressedKeys->keys[keyCode] && MatchModifiers(keysDown, modifier1, modifier2);
}

bool EngineInput::KeyDown(unsigned int keyCode, unsigned int modifier1, unsigned int modifier2)
{
	return keysDown->keys[keyCode] && MatchModifiers(keysDown, modifier1, modifier2);
}

bool EngineInput::KeyJustReleased(unsigned int keyCode, unsigned int modifier1, unsigned int modifier2)
{
	bool downLastFrame = lastKeysDown->keys[keyCode] && MatchModifiers(lastKeysDown, modifier1, modifier2);
	return downLastFrame && (currentReleasedKeys->keys[keyCode] || currentReleasedKeys->keys[modifier1] || currentReleasedKeys->keys[modifier2]);
}