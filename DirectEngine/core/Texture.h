#pragma once

#include <stdint.h>
#include "Memory.h"

#include <iostream>
#include <fstream>

#include <dxgiformat.h>
#include "DDS.h"
using namespace DirectX;

const size_t DDS_MAGIC_BYTE_COUNT = 4;

struct TextureData
{
	size_t width;
	size_t height;
	size_t blockSize;
	size_t mipmapCount;
	DXGI_FORMAT format;
	size_t rowPitch;
	size_t slicePitch;
	uint8_t* data;
	size_t dataLength;
	bool error;
};

TextureData ParseDDSHeader(const wchar_t* path);
TextureData ParseDDS(const wchar_t* path, MemoryArena& arena);