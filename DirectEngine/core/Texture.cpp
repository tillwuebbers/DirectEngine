#include "Texture.h"
#include <assert.h>
#include "../import/stb_image_write.h"

TextureData ParseDDSHeader(const std::string& path)
{
	TextureData result{};

	std::ifstream file(path, std::ios::binary);
	assert(file.good());

	char magic[DDS_MAGIC_BYTE_COUNT];
	file.read(magic, DDS_MAGIC_BYTE_COUNT);
	assert(magic[0] == 'D' && magic[1] == 'D' && magic[2] == 'S' && magic[3] == ' ');

	char headerData[sizeof(DDS_HEADER)];
	file.read(headerData, sizeof(DDS_HEADER));

	DDS_HEADER* header = reinterpret_cast<DDS_HEADER*>(headerData);
	//assert(header->ddspf.flags & DDS_FOURCC);

	bool isCubemap = header->caps2 & DDS_CUBEMAP;

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
		isCubemap = (header10->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE) != 0;
	}

	assert(textureFormat == DXGI_FORMAT_BC1_UNORM_SRGB
		|| textureFormat == DXGI_FORMAT_BC5_UNORM
		|| textureFormat == DXGI_FORMAT_R16G16B16A16_FLOAT);

	result.width = header->width;
	result.height = header->height;
	result.mipmapCount = header->mipMapCount;
	result.format = textureFormat;
	result.viewDimension = isCubemap ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;
	result.arraySize = isCubemap ? 6 : 1;

	if (textureFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		result.blockSize = 16;
		result.rowPitch = result.width * result.blockSize;
		result.slicePitch = result.rowPitch * result.height;
	}
	else
	{
		result.blockSize = textureFormat == DXGI_FORMAT_BC1_UNORM_SRGB ? 8 : 16;
		result.rowPitch = std::max(1ULL, (result.width + 3ULL) / 4ULL) * result.blockSize;
		result.slicePitch = result.rowPitch * result.height;
	}

	file.seekg(128);
	auto dataStart = file.tellg();
	file.seekg(0, std::ios::end);
	result.dataLength = file.tellg() - dataStart;
	result.data = nullptr;
	return result;
}

void SavePNG(const char* path, TextureData& data)
{
	// write with stbi
	stbi_write_hdr(path, data.width, data.height, 4, reinterpret_cast<float*>(data.data));
}
