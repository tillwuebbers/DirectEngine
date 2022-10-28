#pragma once

#include "Memory.h"
#include <mutex>
#include <vector>

const size_t MAX_KEY_CODE = 256;

struct KeyBuffer
{
    bool keys[MAX_KEY_CODE] = { false };

    void Reset();
};

class EngineInput
{
public:
    std::mutex accessMutex = {};
    KeyBuffer* currentPressedKeys;
    KeyBuffer* currentReleasedKeys;
    KeyBuffer* lastKeysDown;
    KeyBuffer* keysDown;

    EngineInput(MemoryArena& arena);

    void NextFrame();
    bool KeyJustPressed(unsigned int keyCode, unsigned int modifier1 = 0, unsigned int modifier2 = 0);
    bool KeyDown(unsigned int keyCode, unsigned int modifier1 = 0, unsigned int modifier2 = 0);
    bool KeyJustReleased(unsigned int keyCode, unsigned int modifier1 = 0, unsigned int modifier2 = 0);
};