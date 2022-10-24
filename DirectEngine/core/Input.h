#pragma once

#include "Memory.h"
#include <mutex>
#include <vector>

const size_t MAX_KEYS_PRESSED = 256;

struct KeyBuffer
{
    unsigned int pressedKeys[MAX_KEYS_PRESSED];
    size_t pressedKeyCount;

    void Set(unsigned int keyCode);
    bool Contains(unsigned int keyCode);
    void Reset();
    ~KeyBuffer() = delete;
};

class EngineInput
{
public:
    std::mutex accessMutex = {};
    KeyBuffer* currentPressedKeys;
    KeyBuffer* previousPressedKeys;

    EngineInput(MemoryArena& arena);

    void NextFrame();
    bool KeyJustPressed(unsigned int keyCode);
    bool KeyDown(unsigned int keyCode);
    bool KeyDownLastFrame(unsigned int keyCode);
    bool KeyJustReleased(unsigned int keyCode);
};