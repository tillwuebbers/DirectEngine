#pragma once

#define GLTF_POSITION "POSITION"
#define GLTF_NORMAL "NORMAL"
#define GLTF_TANGENT "TANGENT"
#define GLTF_TEXCOORD0 "TEXCOORD_0"
#define GLTF_JOINTS "JOINTS_0"
#define GLTF_WEIGHTS "WEIGHTS_0"

#include "../core/Memory.h"
#include "../core/Common.h"
#include "../core/Vertex.h"
using namespace VertexData;

#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>
using namespace DirectX;

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

struct AnimationData
{
	size_t frameCount;
	float* times;
	XMVECTOR* data;
};

struct AnimationJointData
{
	AnimationData translations{};
	AnimationData rotations{};
	AnimationData scales{};
};

struct TransformAnimation
{
	AnimationJointData jointChannels[MAX_BONES];
	bool activeChannels[MAX_BONES];
	size_t channelCount = 0;
	std::string name{};

	bool active = false;
	bool loop = true;
	bool onlyInMainCamera = false;

	float time = 0.f;
	float duration = 0.f;
};

struct TransformHierachy
{
	// Ordered by joint index
	TransformNode nodes[MAX_BONES];
	size_t nodeCount;
	TransformNode* root;
	TransformAnimation animations[MAX_ANIMATIONS];
	std::unordered_map<std::string, size_t> animationNameToIndex{};

	size_t animationCount;
	// nodeIdx = jointToNodeIndex[jointIdx]
	size_t jointToNodeIndex[MAX_BONES];
	// jointIdx = nodeToJointIndex[nodeIdx]
	size_t nodeToJointIndex[MAX_BONES];

	void UpdateNode(TransformNode* node);
	void SetAnimationActive(std::string name, bool state);
};

struct GltfResult
{
	std::vector<MeshFile> meshes{};
	TransformHierachy* transformHierachy = nullptr;
};

MeshFile CreateQuad(float width, float height, MemoryArena& arena);
MeshFile CreateQuadY(float width, float height, MemoryArena& arena);
GltfResult LoadGltfFromFile(const std::string& filePath, MemoryArena& arena);