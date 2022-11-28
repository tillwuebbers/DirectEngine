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
	uint32_t boneWeights;
	uint32_t boneIndices;
};

struct TransformNode
{
	XMMATRIX local;
	XMMATRIX global;
	TransformNode* children[MAX_CHILDREN];
	size_t childCount;
};

struct TransformHierachy
{
	TransformNode nodes[MAX_BONES];
	TransformNode* root;
};

struct MeshFile
{
	Vertex* vertices;
	size_t vertexCount;
	XMMATRIX* bones;
	size_t boneCount;
	TransformHierachy* hierachy;
};

MeshFile CreateQuad(float width, float height, MemoryArena& vertexArena);
MeshFile LoadGltfFromFile(const std::string& filePath, RingLog& debugLog, MemoryArena& vertexArena, MemoryArena& boneArena);