#pragma once

#include <cstdint>

// Crazy custom memory experiments, dangerous? (and fun)
// WARNING: Anything allocated inside a memory arena won't get it's desctructor called (intentionally).
// Don't store std::string or similar in here!
class MemoryArena
{
public:
    const size_t capacity = 0;
    size_t allocationGranularity = 1024 * 64;

    uint8_t* base;
    size_t used = 0;
    size_t committed = 0;

    MemoryArena(size_t capacity = 1024 * 1024 * 1024);
    void* Allocate(size_t size);
    void Reset(bool freePages = false);

    MemoryArena(const MemoryArena& other) = delete;
    MemoryArena(MemoryArena&& other) noexcept = delete;
    MemoryArena& operator=(const MemoryArena& other) = delete;
    MemoryArena& operator=(MemoryArena&& other) noexcept = delete;
    ~MemoryArena();
};

#define NewObject(arena, type, ...) new(arena.Allocate(sizeof(type))) type(__VA_ARGS__);
#define NewArray(arena, type, count, ...) new(arena.Allocate(sizeof(type) * count)) type[count](__VA_ARGS__);