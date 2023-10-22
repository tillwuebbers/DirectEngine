#include "Texture.h"
#include <assert.h>

TextureData ParseDDSHeader(const wchar_t* path)
{
	TextureData result{};

	std::ifstream file(path, std::ios::binary);

	char magic[DDS_MAGIC_BYTE_COUNT];
	file.read(magic, DDS_MAGIC_BYTE_COUNT);
	assert(magic[0] == 'D' && magic[1] == 'D' && magic[2] == 'S' && magic[3] == ' ');

	char headerData[sizeof(DDS_HEADER)];
	file.read(headerData, sizeof(DDS_HEADER));

	DDS_HEADER* header = reinterpret_cast<DDS_HEADER*>(headerData);
	assert(header->ddspf.flags & DDS_FOURCC);

	DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
	if (header->ddspf.fourCC == MAKEFOURCC('D', 'X', 'T', '1'))
	{
		textureFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
	}
	else if (header->ddspf.fourCC == MAKEFOURCC('D', 'X', '1', '0'))
	{
		char dx10HeaderData[sizeof(DDS_HEADER_DXT10)];
		file.read(dx10HeaderData, sizeof(DDS_HEADER_DXT10));
		DDS_HEADER_DXT10* header10 = reinterpret_cast<DDS_HEADER_DXT10*>(dx10HeaderData);
		textureFormat = header10->dxgiFormat;
	}

	assert(textureFormat == DXGI_FORMAT_BC1_UNORM_SRGB || textureFormat == DXGI_FORMAT_BC5_UNORM);

	result.width = header->width;
	result.height = header->height;
	result.blockSize = textureFormat == DXGI_FORMAT_BC1_UNORM_SRGB ? 8 : 16;
	result.mipmapCount = header->mipMapCount;
	result.format = textureFormat;
	result.rowPitch = std::max(1ULL, (result.width + 3ULL) / 4ULL) * result.blockSize;
	result.slicePitch = result.rowPitch * result.height;

	file.seekg(128);
	auto dataStart = file.tellg();
	file.seekg(0, std::ios::end);
	result.dataLength = file.tellg() - dataStart;
	result.data = nullptr;
	return result;
}

TextureData ParseDDS(const wchar_t* path, MemoryArena& arena)
{
	TextureData result{};

	std::ifstream file(path, std::ios::binary);

	char magic[DDS_MAGIC_BYTE_COUNT];
	file.read(magic, DDS_MAGIC_BYTE_COUNT);

	char headerData[sizeof(DDS_HEADER)];
	file.read(headerData, sizeof(DDS_HEADER));

	DDS_HEADER* header = reinterpret_cast<DDS_HEADER*>(headerData);
	result.width = header->width;
	result.height = header->height;
	result.blockSize = 8; // BC1
	result.mipmapCount = header->mipMapCount;
	result.format = DXGI_FORMAT_BC1_UNORM_SRGB;
	result.rowPitch = std::max(1ULL, (result.width + 3ULL) / 4ULL) * result.blockSize;
	result.slicePitch = result.rowPitch * result.height;

	file.seekg(128);
	auto dataStart = file.tellg();
	file.seekg(0, std::ios::end);
	result.dataLength = file.tellg() - dataStart;

	result.data = NewArray(arena, uint8_t, result.dataLength);

	file.seekg(128);
	file.read(reinterpret_cast<char*>(result.data), result.dataLength);

	return result;
}