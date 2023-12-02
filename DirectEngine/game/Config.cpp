#include "Config.h"

bool LoadConfig(MemoryArena& arena, const char* path)
{
	arena.Reset();
	FILE* fileHandle = fopen(path, "rb");
	if (fileHandle == nullptr) return false;

	fseek(fileHandle, 0L, SEEK_END);
	size_t fileSize = ftell(fileHandle);
	rewind(fileHandle);

	arena.AllocateRaw(fileSize);
	size_t readObjects = fread(arena.base, fileSize, 1, fileHandle);
	assert(readObjects > 0);
	fclose(fileHandle);

	return true;
}

bool SaveConfig(MemoryArena& arena, const char* path)
{
	assert(arena.used > 0);
	FILE* fileHandle = fopen(path, "wb");
	if (fileHandle == nullptr) return false;

	size_t writtenObjects = fwrite(arena.base, arena.used, 1, fileHandle);
	assert(writtenObjects > 0);
	fclose(fileHandle);

	return true;
}
