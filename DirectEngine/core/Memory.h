#pragma once

#include <cstdint>

// Crazy custom memory experiments, dangerous? (and fun)
// WARNING: Anything allocated inside a memory arena won't get it's desctructor called (intentionally)
class MemoryArena
{
public:
    uint8_t* base;
    const size_t capacity = 0;
    size_t used = 0;

    MemoryArena(size_t capacity);
    void* Allocate(size_t size);
    void Reset();

    MemoryArena(const MemoryArena& other) = delete;
    MemoryArena(MemoryArena&& other) noexcept = delete;
    MemoryArena& operator=(const MemoryArena& other) = delete;
    MemoryArena& operator=(MemoryArena&& other) noexcept = delete;
    ~MemoryArena();
};

#define NewObject(arena, type, ...) new(arena.Allocate(sizeof(type))) type(__VA_ARGS__);
