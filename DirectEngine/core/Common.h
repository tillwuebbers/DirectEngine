#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
using Microsoft::WRL::ComPtr;

#include <d3d12.h>
#include <d3dx12.h>

struct DescriptorHandle
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};

struct Texture
{
    ComPtr<ID3D12Resource> buffer = nullptr;
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
    ShaderDescription shaderDescription;
    size_t textureSlotCount;

    ComPtr<ID3D12PipelineState> pipelineState;
    ComPtr<ID3D12RootSignature> rootSignature;

    HRESULT creationError;
};