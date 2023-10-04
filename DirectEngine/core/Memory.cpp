#include "Memory.h"
#include <assert.h>
#include <cstdlib>

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

MemoryArena::MemoryArena(size_t capacity) :
	capacity(capacity),
	base(static_cast<uint8_t*>(VirtualAlloc(NULL, capacity, MEM_RESERVE, PAGE_READWRITE)))
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	allocationGranularity = info.dwAllocationGranularity;
}

size_t Align(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

void* MemoryArena::Allocate(size_t size)
{
	assert(size >= 0);
	const size_t newUsed = used + size;
	assert(newUsed <= capacity);

	if (newUsed > committed)
	{
		const size_t required = newUsed - committed;
		const size_t allocationSize = Align(required, allocationGranularity);

		VirtualAlloc(base + committed, allocationSize, MEM_COMMIT, PAGE_READWRITE);
		committed += allocationSize;
	}

	uint8_t* result = base + used;
	used = newUsed;
	return result;
}

void* MemoryArena::AllocateAligned(size_t size, size_t alignment)
{
	size_t difference = Align(used, alignment) - used;
	Allocate(difference);
	return Allocate(size);
}

void MemoryArena::Reset(bool freePages)
{
	if (freePages)
	{
		if (!VirtualFree(base, committed, MEM_DECOMMIT))
		{
			OutputDebugString(L"Failed to reset Memory Arena!!");
		}
		committed = 0;
	}
	used = 0;
}

MemoryArena::~MemoryArena()
{
	if (!VirtualFree(base, 0, MEM_RELEASE))
	{
		OutputDebugString(L"Failed to free Memory Arena!!");
	}
}
