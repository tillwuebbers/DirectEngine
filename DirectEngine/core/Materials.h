#include "Memory.h"
#include "Common.h"
#include <string>

#include "DirectXMath.h"
using namespace DirectX;

enum class ShaderType
{
	Rasterize,
	Compute,
	Raytrace,
};

struct ShaderDefine
{
	FixedStr name;
	FixedStr value;
};

struct StandaloneShaderFile
{
	FixedStr shaderName = "";
	StackArray<ShaderDefine, MAX_DEFINES_PER_MATERIAL> defines = {};
	ShaderType type = ShaderType::Rasterize;
};

struct TextureFile
{
	FixedStr texturePath = "";
	uint64_t textureHash = 0;
	TextureGPU* textureGPU = nullptr;
	bool isSRGB = false;
};

struct MaterialFile
{
	FixedStr materialName = "";
	uint64_t materialHash = 0;
	FixedStr shaderName = "";
	TextureFile* diffuseTexture = nullptr;
	TextureFile* normalTexture = nullptr;
	TextureFile* metallicRoughnessTexture = nullptr;
	XMVECTOR diffuseColor = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
	MaterialData* data = nullptr;
	StackArray<ShaderDefine, MAX_DEFINES_PER_MATERIAL> defines = {};
	StackArray<RootConstantInfo, MAX_ROOT_CONSTANTS_PER_MATERIAL> rootConstants = {};
};

void LoadMaterials(const std::string& assetListFilePath, ArenaArray<MaterialFile>& materials, ArenaArray<TextureFile>& textures, ArenaArray<StandaloneShaderFile>& standaloneShaders);