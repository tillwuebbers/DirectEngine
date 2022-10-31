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
    int mouseDeltaAccX = 0;
    int mouseDeltaAccY = 0;

    float mouseX = 0.;
    float mouseY = 0.;
    float mouseDeltaX = 0.;
    float mouseDeltaY = 0.;

    EngineInput(MemoryArena& arena);

    void UpdateMousePosition();
    void NextFrame();
    bool KeyJustPressed(unsigned int keyCode, unsigned int modifier1 = 0, unsigned int modifier2 = 0);
    bool KeyDown(unsigned int keyCode, unsigned int modifier1 = 0, unsigned int modifier2 = 0);
    bool KeyJustReleased(unsigned int keyCode, unsigned int modifier1 = 0, unsigned int modifier2 = 0);
};