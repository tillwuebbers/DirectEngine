#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include "Memory.h"
#include <assert.h>
#include <cstdlib>

MemoryArena::MemoryArena(size_t capacity) :
	capacity(capacity),
	base(static_cast<uint8_t*>(VirtualAlloc(NULL, capacity, MEM_RESERVE, PAGE_READWRITE)))
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	allocationGranularity = info.dwAllocationGranularity;
}

void* MemoryArena::Allocate(size_t size)
{
	const size_t newUsed = used + size;
	assert(newUsed <= capacity);

	if (newUsed > committed)
	{
		const size_t required = newUsed - committed;
		// TODO: round committed bytes to page boundaries automatically?
		//const size_t commitAmount = ...;

		VirtualAlloc(base + committed, required, MEM_COMMIT, PAGE_READWRITE);
		committed += required;
	}

	uint8_t* result = base + used;
	used = newUsed;
	return result;
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