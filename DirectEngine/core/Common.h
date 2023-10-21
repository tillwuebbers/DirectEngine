#pragma once

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>
using namespace DirectX;

#include "Constants.h"

struct DescriptorHandle
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
};

struct Texture
{
    ID3D12Resource* buffer = nullptr;
    DescriptorHandle handle = {};
    std::wstring name = {};
};

class PipelineConfig
{
public:
    PipelineConfig(const std::wstring& shaderName, size_t textureSlotCount)
    {
		this->shaderName = shaderName;
        this->textureSlotCount = textureSlotCount;
    }

    bool shadow = false;
    bool wireframe = false;
    bool ignoreDepth = false;
    std::wstring shaderName;
    size_t textureSlotCount = 0;
    UINT sampleCount = 1;
    UINT renderTargetCount = 1;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    DXGI_FORMAT format = DISPLAY_FORMAT;

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12RootSignature* rootSignature = nullptr;

    HRESULT creationError = ERROR_SUCCESS;
};