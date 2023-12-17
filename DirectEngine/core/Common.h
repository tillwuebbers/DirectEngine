#pragma once

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>
using namespace DirectX;

#include "Memory.h"
#include "Constants.h"

struct DescriptorHandle
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
};

struct TextureGPU
{
    ID3D12Resource* buffer = nullptr;
    DescriptorHandle handle = {};
    std::string name = {};
};

class PipelineConfig
{
public:
    PipelineConfig(const std::string& shaderName, size_t textureSlotCount)
    {
		this->shaderName = shaderName;
        this->textureSlotCount = textureSlotCount;
    }

    bool ignoreDepth = false;
    std::string shaderName;
    size_t textureSlotCount = 0;
    size_t rootConstantCount = 0;
    UINT sampleCount = 1;
    UINT renderTargetCount = 1;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
    DXGI_FORMAT format = DISPLAY_FORMAT;

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12RootSignature* rootSignature = nullptr;

    HRESULT creationError = ERROR_SUCCESS;
};

enum class RootConstantType
{
    UINT = 0,
    FLOAT = 1,
};

struct RootConstantInfo
{
    RootConstantType type = RootConstantType::UINT;
    FixedStr name = "-";
    uint32_t defaultValue = 0;
};

struct EntityData;
class MaterialData
{
public:
    StackArray<EntityData*, MAX_ENTITIES_PER_MATERIAL> entities = {};
    StackArray<TextureGPU*, MAX_TEXTURES_PER_MATERIAL> textures = {};
    StackArray<RootConstantInfo, MAX_ROOT_CONSTANTS_PER_MATERIAL> rootConstants = {};
    UINT rootConstantData[MAX_ROOT_CONSTANTS_PER_MATERIAL] = {};
    PipelineConfig* pipeline = nullptr;
    std::string name = "Material";
    size_t shellCount = 0;

    template <typename T>
    void SetRootConstant(size_t index, T value)
    {
        assert(index < rootConstants.size);
        T* rootConstAddr = reinterpret_cast<T*>(&rootConstantData[index]);
        *rootConstAddr = value;
    }
};
