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

struct PipelineConfig
{
    bool wireframe;
    bool ignoreDepth;
    ShaderDescription shaderDescription;
    size_t textureSlotCount;

    ID3D12PipelineState* pipelineState;
    ID3D12RootSignature* rootSignature;

    HRESULT creationError;
};