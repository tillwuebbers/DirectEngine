#include "Mesh.h"

#include <format>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "../import/tiny_gltf.h"
using namespace tinygltf;

MeshFile CreateQuad(float width, float height, MemoryArena& vertexArena)
{
	const size_t VERTEX_COUNT = 6;

	Vertex* vertices = NewArray(vertexArena, Vertex, VERTEX_COUNT);
	vertices[0] = { { 0.f  , 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {0.f, 0.f}, {}, {} };
	vertices[1] = { { width, 0.f, height }, {}, {0.f, 1.f, 0.f }, {1.f, 1.f}, {}, {} };
	vertices[2] = { { width, 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f}, {}, {} };
	vertices[3] = { { 0.f  , 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {0.f, 0.f}, {}, {} };
	vertices[4] = { { 0.f  , 0.f, height }, {}, {0.f, 1.f, 0.f }, {0.f, 1.f}, {}, {} };
	vertices[5] = { { width, 0.f, height }, {}, {0.f, 1.f, 0.f }, {1.f, 1.f}, {}, {} };

	return MeshFile{ vertices, VERTEX_COUNT, 0, nullptr };
}

template <typename T>
const T* ReadBuffer(Model& model, Accessor& accessor)
{
	BufferView& bufferView = model.bufferViews[accessor.bufferView];
	Buffer& buffer = model.buffers[bufferView.buffer];
	return reinterpret_cast<T*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
}

XMVECTOR LoadTranslation(std::vector<double>& translationVec)
{
	if (translationVec.size() == 0) return { 1.f, 1.f, 1.f };
	return { static_cast<float>(translationVec[0]), static_cast<float>(translationVec[1]), static_cast<float>(translationVec[2]) };
}

XMVECTOR LoadRotation(std::vector<double>& quaternionVec)
{
	if (quaternionVec.size() == 0) return XMQuaternionIdentity();
	return { static_cast<float>(quaternionVec[0]), static_cast<float>(quaternionVec[1]), static_cast<float>(quaternionVec[2]), static_cast<float>(quaternionVec[3]) };
}

XMVECTOR LoadScale(std::vector<double>& scaleVec)
{
	if (scaleVec.size() == 0) return { 1.f, 1.f, 1.f };
	return { static_cast<float>(scaleVec[0]), static_cast<float>(scaleVec[1]), static_cast<float>(scaleVec[2]) };
}

TransformNode* CreateMatrices(Model& model, int jointIndex, TransformNode* parent, TransformNode* nodeList, const float* inverseBindMatrixData)
{
	Node& node = model.nodes[model.skins[0].joints[jointIndex]];

	TransformNode& transformNode = nodeList[jointIndex];
	transformNode.baseLocal = DirectX::XMMatrixAffineTransformation(LoadScale(node.scale), XMVECTOR{}, LoadRotation(node.rotation), LoadTranslation(node.translation));
	transformNode.currentLocal = transformNode.baseLocal;
	transformNode.parent = parent;

	const float* ibmData = &inverseBindMatrixData[jointIndex * 16];
	transformNode.inverseBind = XMMATRIX(
		static_cast<float>(ibmData[0]), static_cast<float>(ibmData[1]), static_cast<float>(ibmData[2]), static_cast<float>(ibmData[3]),
		static_cast<float>(ibmData[4]), static_cast<float>(ibmData[5]), static_cast<float>(ibmData[6]), static_cast<float>(ibmData[7]),
		static_cast<float>(ibmData[8]), static_cast<float>(ibmData[9]), static_cast<float>(ibmData[10]), static_cast<float>(ibmData[11]),
		static_cast<float>(ibmData[12]), static_cast<float>(ibmData[13]), static_cast<float>(ibmData[14]), static_cast<float>(ibmData[15])
	);

	if (parent != nullptr)
	{
		transformNode.global = XMMatrixMultiply(transformNode.currentLocal, parent->global);
	}
	else
	{
		transformNode.global = transformNode.currentLocal;
	}

	for (int childNodeIndex : node.children)
	{
		assert(transformNode.childCount < MAX_CHILDREN);

		auto& jointVec = model.skins[0].joints;
		auto childJoint = std::find(jointVec.begin(), jointVec.end(), childNodeIndex);
		if (childJoint != jointVec.end())
		{
			size_t childJointIndex = childJoint - jointVec.begin();
			transformNode.children[transformNode.childCount] = CreateMatrices(model, childJointIndex, &transformNode, nodeList, inverseBindMatrixData);
			transformNode.childCount++;
		}
	}
	return &nodeList[jointIndex];
}

std::vector<MeshFile> LoadGltfFromFile(const std::string& filePath, RingLog& debugLog, MemoryArena& vertexArena, MemoryArena& boneArena)
{
	Model model;
	TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);

	if (!warn.empty())
	{
		OutputDebugStringA(std::format("Warn: {}\n", warn).c_str());
	}

	if (!err.empty())
	{
		OutputDebugStringA(std::format("Err: {}\n", err).c_str());
	}

	if (!ret)
	{
		OutputDebugStringA(std::format("Failed to parse glTF\n").c_str());
	}

	size_t vertexCount = 0;
	Vertex* vertices;

	size_t boneCount = model.skins[0].joints.size();

	Accessor& inverseBindAccessor = model.accessors[model.skins[0].inverseBindMatrices];
	assert(inverseBindAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
	assert(inverseBindAccessor.type == TINYGLTF_TYPE_MAT4);
	const float* inverseBindMatrices = ReadBuffer<float>(model, inverseBindAccessor);

	TransformHierachy* hierachy = NewObject(boneArena, TransformHierachy);
	hierachy->root = CreateMatrices(model, 0, nullptr, hierachy->nodes, inverseBindMatrices);

	std::vector<MeshFile> meshFiles{};

	for (Mesh& mesh : model.meshes)
	{
		for (Primitive& primitive : mesh.primitives)
		{
			Accessor& indexAccessor = model.accessors[primitive.indices];
			assert(indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
			assert(indexAccessor.type == TINYGLTF_TYPE_SCALAR);
			const uint16_t* indexData = ReadBuffer<uint16_t>(model, indexAccessor);

			Accessor& positionAccessor = model.accessors[primitive.attributes[GLTF_POSITION]];
			assert(positionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
			assert(positionAccessor.type == TINYGLTF_TYPE_VEC3);
			const float* positionData = ReadBuffer<float>(model, positionAccessor);

			Accessor& normalAccessor = model.accessors[primitive.attributes[GLTF_NORMAL]];
			assert(normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
			assert(normalAccessor.type == TINYGLTF_TYPE_VEC3);
			const float* normalData = ReadBuffer<float>(model, normalAccessor);

			Accessor& uvAccessor = model.accessors[primitive.attributes[GLTF_TEXCOORD0]];
			assert(uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
			assert(uvAccessor.type == TINYGLTF_TYPE_VEC2);
			const float* uvData = ReadBuffer<float>(model, uvAccessor);

			Accessor& jointAccessor = model.accessors[primitive.attributes[GLTF_JOINTS]];
			assert(jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
			assert(jointAccessor.type == TINYGLTF_TYPE_VEC4);
			const uint8_t* jointData = ReadBuffer<uint8_t>(model, jointAccessor);

			Accessor& weightAccessor = model.accessors[primitive.attributes[GLTF_WEIGHTS]];
			assert(weightAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
			assert(weightAccessor.type == TINYGLTF_TYPE_VEC4);
			const float* weightData = ReadBuffer<float>(model, weightAccessor);

			vertices = NewArray(vertexArena, Vertex, indexAccessor.count);
			vertexCount = indexAccessor.count;

			for (int i = 0; i < vertexCount; i++)
			{
				uint16_t idx = indexData[i];
				assert(idx < positionAccessor.count);
				assert(idx < normalAccessor.count);
				assert(idx < uvAccessor.count);
				assert(idx < jointAccessor.count);
				assert(idx < weightAccessor.count);
				Vertex& vert = vertices[i];

				vert.position.x = positionData[idx * 3 + 0];
				vert.position.y = positionData[idx * 3 + 1];
				vert.position.z = positionData[idx * 3 + 2];

				vert.normal.x = normalData[idx * 3 + 0];
				vert.normal.y = normalData[idx * 3 + 1];
				vert.normal.z = normalData[idx * 3 + 2];

				vert.uv.x = uvData[idx * 2 + 0];
				vert.uv.y = uvData[idx * 2 + 1];

				vert.boneIndices.x = jointData[idx * 4 + 0];
				vert.boneIndices.y = jointData[idx * 4 + 1];
				vert.boneIndices.z = jointData[idx * 4 + 2];
				vert.boneIndices.w = jointData[idx * 4 + 3];

				float w0 = weightData[idx * 4 + 0];
				float w1 = weightData[idx * 4 + 1];
				float w2 = weightData[idx * 4 + 2];
				float w3 = weightData[idx * 4 + 3];
				assert(abs(1. - (w0 + w1 + w2 + w3)) < 0.01);

				vert.boneWeights.x = w0;
				vert.boneWeights.y = w1;
				vert.boneWeights.z = w2;
				vert.boneWeights.w = w3;
			}

			std::string imageName{};
			if (model.materials.size() > 0 && model.textures.size() > 0 && model.images.size() > 0)
			{
				assert(model.materials.size() > primitive.material);
				int baseColorIndex = model.materials[primitive.material].pbrMetallicRoughness.baseColorTexture.index;
				
				assert(model.textures.size() > baseColorIndex);
				int imageIndex = model.textures[baseColorIndex].source;
				
				assert(model.images.size() > imageIndex);
				imageName = model.images[imageIndex].name;
				imageName.append(".dds");
			}

			meshFiles.emplace_back(MeshFile{ vertices, vertexCount, boneCount, hierachy, imageName });
		}
	}

	return meshFiles;
}

void TransformHierachy::UpdateNode(TransformNode* node)
{
	assert(node != nullptr);
	if (node->parent != nullptr)
	{
		node->global = XMMatrixMultiply(node->currentLocal, node->parent->global);
	}

	for (int i = 0; i < node->childCount; i++)
	{
		UpdateNode(node->children[i]);
	}
}
