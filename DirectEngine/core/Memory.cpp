#include "Memory.h"

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

void* MemoryArena::AllocateRaw(size_t byteCount, size_t alignment)
{
	const size_t alignedUsed = Align(used, std::max<size_t>(alignment, MIN_ALIGN));
	const size_t newUsed = alignedUsed + byteCount;
	assert(newUsed <= capacity);

	if (newUsed > committed)
	{
		const size_t required = newUsed - committed;
		const size_t allocationSize = Align(required, allocationGranularity);

		VirtualAlloc(base + committed, allocationSize, MEM_COMMIT, PAGE_READWRITE);
		committed += allocationSize;
	}

	used = newUsed;
	return base + alignedUsed;
}

size_t Align(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
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
