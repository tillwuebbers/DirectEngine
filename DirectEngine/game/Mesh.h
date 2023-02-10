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
	XMFLOAT4 boneWeights;
	XMUINT4 boneIndices;
};

struct TransformNode
{
	XMMATRIX inverseBind;
	XMMATRIX baseLocal;
	XMMATRIX currentLocal;
	XMMATRIX global;
	TransformNode* parent;
	TransformNode* children[MAX_CHILDREN];
	size_t childCount;
	std::string name;
};

struct TransformAnimationChannel
{
	float* times;
	XMVECTOR* rotations;
	size_t frameCount;
	size_t nodeIndex;
};

struct TransformAnimation
{
	TransformAnimationChannel channels[MAX_ANIMATION_CHANNELS];
	size_t channelCount;
	float duration;
};

struct TransformHierachy
{
	// Ordered by joint index
	TransformNode nodes[MAX_BONES];
	size_t nodeCount;
	TransformNode* root;
	TransformAnimation animations[MAX_ANIMATIONS];
	size_t animationCount;
	// nodeIdx = jointToNodeIndex[jointIdx]
	size_t jointToNodeIndex[MAX_BONES];

	void UpdateNode(TransformNode* node);
};

struct MeshFile
{
	Vertex* vertices;
	size_t vertexCount = 0;
	TransformHierachy* hierachy;
	std::string textureName{};
};

MeshFile CreateQuad(float width, float height, MemoryArena& vertexArena);
std::vector<MeshFile> LoadGltfFromFile(const std::string& filePath, RingLog& debugLog, MemoryArena& vertexArena, MemoryArena& boneArena, MemoryArena& aniamtionArena);