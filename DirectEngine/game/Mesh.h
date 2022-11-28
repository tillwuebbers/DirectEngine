#pragma once

#define GLTF_POSITION "POSITION"
#define GLTF_NORMAL "NORMAL"
#define GLTF_TEXCOORD0 "TEXCOORD_0"
#define GLTF_JOINTS "JOINTS_0"
#define GLTF_WEIGHTS "WEIGHTS_0"

#include "Log.h"
#include "../core/Memory.h"

#include <string>
#include <vector>
#include <DirectXMath.h>
using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
	uint64_t boneWeights;
	uint64_t boneIndices;
};

struct MeshFile
{
	Vertex* vertices;
	size_t vertexCount;
	uint64_t* indices;
	size_t indexCount;
};

MeshFile CreateQuad(float width, float height, MemoryArena& vertexArena, MemoryArena& indexArena);
MeshFile LoadObjFromFile(const std::string& filePath, const std::string& materialPath, RingLog& debugLog, MemoryArena& vertexArena, MemoryArena& indexArena);
MeshFile LoadGltfFromFile(const std::string& filePath, RingLog& debugLog, MemoryArena& vertexArena);