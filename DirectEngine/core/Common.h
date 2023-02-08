#pragma once

#include <d3d12.h>
#include <d3dx12.h>

struct DescriptorHandle
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};

struct Texture
{
    ID3D12Resource* buffer = nullptr;
    DescriptorHandle handle;
};

struct ShaderDescription
{
    const wchar_t* shaderFileName;
    const char* vsEntryName;
    const char* psEntryName;
    const wchar_t* debugName;
};

class PipelineConfig
{
public:
    PipelineConfig(ShaderDescription shaderDescription, size_t textureSlotCount)
    {
		this->shaderDescription = shaderDescription;
		this->textureSlotCount = textureSlotCount;
    }

    bool wireframe = false;
    bool ignoreDepth = false;
    ShaderDescription shaderDescription;
    size_t textureSlotCount = 0;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12RootSignature* rootSignature = nullptr;

    HRESULT creationError = ERROR_SUCCESS;
};