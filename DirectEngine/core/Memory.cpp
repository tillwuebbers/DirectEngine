#include "Memory.h"
#include <assert.h>
#include <cstdlib>

MemoryArena::MemoryArena(size_t capacity) :
	capacity(capacity),
	base(static_cast<uint8_t*>(malloc(capacity)))
{}

void* MemoryArena::Allocate(size_t size)
{
	// TODO: 4-byte align?
	assert(used + size <= capacity);
	uint8_t* result = base + used;
	used += size;
	return result;
}

void MemoryArena::Reset()
{
	used = 0;
}

MemoryArena::~MemoryArena()
{
	free(base);
}