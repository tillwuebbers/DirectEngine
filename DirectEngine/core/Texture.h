#pragma once

#include <stdint.h>
#include "Memory.h"

#include <iostream>
#include <fstream>

#include <d3d12.h>
#include <dxgiformat.h>
#include "DDS.h"
using namespace DirectX;

#undef min
#undef max

const size_t DDS_MAGIC_BYTE_COUNT = 4;

struct TextureData
{
	size_t width;
	size_t height;
	size_t arraySize;
	size_t blockSize;
	size_t mipmapCount;
	DXGI_FORMAT format;
	D3D12_SRV_DIMENSION viewDimension;
	size_t rowPitch;
	size_t slicePitch;
	uint8_t* data;
	size_t dataLength;
	bool error;
};

TextureData ParseDDSHeader(const std::string& path);
void SavePNG(const char* path, TextureData& data);