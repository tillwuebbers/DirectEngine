#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>
#include "Constants.h"
using namespace DirectX;

namespace VertexData
{
    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
        XMFLOAT3 normal;
        XMFLOAT3 tangent;
        XMFLOAT3 bitangent;
        XMFLOAT2 uv;
        XMFLOAT4 boneWeights;
        XMUINT4 boneIndices;
    };

    struct MeshFile
    {
        VertexData::Vertex* vertices = nullptr;
        size_t vertexCount = 0;
        INDEX_BUFFER_TYPE* indices = nullptr;
        size_t indexCount = 0;
        std::string textureName{};
        size_t textureCount = 0; // TODO use better solution
    };

    constexpr D3D12_INPUT_ELEMENT_DESC VERTEX_DESCS[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BITANGENT",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 52, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "UV",           0, DXGI_FORMAT_R32G32_FLOAT,       0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 72, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 88, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}
