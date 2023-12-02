#include "Mesh.h"
#include "../Helpers.h"
#include "../core/Log.h"
#include "../core/Vertex.h"
using namespace VertexData;

#include <format>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "../import/tiny_gltf.h"

using namespace tinygltf;
using namespace std::chrono;

MeshFile CreateQuad(float width, float height, MemoryArena& arena)
{
	uint64_t hash = 0;
	Vertex* vertices = NewArray(arena, Vertex, 4);
	vertices[0] = { { 0.f  , 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f}, {}, {} };
	vertices[1] = { { width, 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f}, {}, {} };
	vertices[2] = { { 0.f  , 0.f, height }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f}, {}, {} };
	vertices[3] = { { width, 0.f, height }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f}, {}, {} };

	INDEX_BUFFER_TYPE* indices = NewArray(arena, INDEX_BUFFER_TYPE, 6);
	indices[0] = 0;
	indices[1] = 3;
	indices[2] = 1;
	indices[3] = 0;
	indices[4] = 2;
	indices[5] = 3;

	return MeshFile{ vertices, 4, indices, 6, "", 0 };
}

MeshFile CreateQuadY(float width, float height, MemoryArena& arena)
{
	uint64_t hash = 0;
	Vertex* vertices = NewArray(arena, Vertex, 4);
	vertices[0] = { { 0.f  , 0.f   , 0.f }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f}, {}, {} };
	vertices[1] = { { width, 0.f   , 0.f }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f}, {}, {} };
	vertices[2] = { { 0.f  , height, 0.f }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f}, {}, {} };
	vertices[3] = { { width, height, 0.f }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f}, {}, {} };

	INDEX_BUFFER_TYPE* indices = NewArray(arena, INDEX_BUFFER_TYPE, 6);
	indices[0] = 0;
	indices[1] = 3;
	indices[2] = 1;
	indices[3] = 0;
	indices[4] = 2;
	indices[5] = 3;

	return MeshFile{ vertices, 4, indices, 6, "", 0 };
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
	size_t nodeIndex = model.skins[0].joints[jointIndex];
	Node& node = model.nodes[nodeIndex];

	TransformNode& transformNode = nodeList[jointIndex];
	transformNode.baseLocal = DirectX::XMMatrixAffineTransformation(LoadScale(node.scale), XMVECTOR{}, LoadRotation(node.rotation), LoadTranslation(node.translation));
	transformNode.currentLocal = transformNode.baseLocal;
	transformNode.parent = parent;
	transformNode.name = node.name;

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

Accessor& CheckAccessor(tinygltf::Model& model, Primitive& primitive, const char* attribute, int type)
{
	assert(primitive.attributes.contains(attribute));
	Accessor& accessor = model.accessors[primitive.attributes[attribute]];
	assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
	assert(accessor.type == type);
	return accessor;
}

GltfResult LoadGltfFromFile(const std::string& filePath, MemoryArena& arena)
{
	INIT_TIMER(timer);

	Model model;
	TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);

	if (!warn.empty())
	{
		WARN(warn);
	}

	if (!err.empty())
	{
		ERR(err);
	}

	if (!ret)
	{
		ERR("Failed to parse glTF");
	}

	size_t vertexCount = 0;
	Vertex* vertices;
	size_t indexCount = 0;
	INDEX_BUFFER_TYPE* indices;

	LOG_TIMER(timer, "Load Model Binary");
	RESET_TIMER(timer);

	TransformHierachy* hierachy = nullptr;

	if (model.skins.size() > 0)
	{
		Accessor& inverseBindAccessor = model.accessors[model.skins[0].inverseBindMatrices];
		assert(inverseBindAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
		assert(inverseBindAccessor.type == TINYGLTF_TYPE_MAT4);
		const float* inverseBindMatrices = ReadBuffer<float>(model, inverseBindAccessor);

		hierachy = NewObject(arena, TransformHierachy);
		hierachy->nodeCount = model.skins[0].joints.size();
		for (int i = 0; i < model.skins[0].joints.size(); i++)
		{
			hierachy->jointToNodeIndex[i] = model.skins[0].joints[i];
		}
		for (int i = 0; i < model.nodes.size(); i++)
		{
			for (int j = 0; j < model.skins[0].joints.size(); j++)
			{
				if (model.skins[0].joints[j] == i)
				{
					hierachy->nodeToJointIndex[i] = j;
					break;
				}
			}
		}
		hierachy->root = CreateMatrices(model, 0, nullptr, hierachy->nodes, inverseBindMatrices);

		LOG_TIMER(timer, "Load Hierachy");
		RESET_TIMER(timer);

		for (Animation& animation : model.animations)
		{
			assert(hierachy->animationCount < MAX_ANIMATIONS);
			TransformAnimation& transformAnimation = hierachy->animations[hierachy->animationCount] = {};
			transformAnimation.name = animation.name;

			hierachy->animationNameToIndex.insert({ animation.name, hierachy->animationCount });
			hierachy->animationCount++;

			std::vector<std::string> maskedChannels{};
			if (animation.extras.Has("mask"))
			{
				maskedChannels.push_back(animation.extras.Get("mask").Get<std::string>());
			}
			if (animation.extras.Has("mainrender"))
			{
				transformAnimation.onlyInMainCamera = true;
			}


			float maxTime = 0.f;

			for (AnimationChannel& channel : animation.channels)
			{
				// Skip if channel is not in our mask
				bool& channelActive = transformAnimation.activeChannels[channel.target_node];
				if (maskedChannels.size() > 0)
				{
					std::string& channelNodeName = hierachy->nodes[channel.target_node].name;
					if (std::find(maskedChannels.begin(), maskedChannels.end(), channelNodeName) == maskedChannels.end())
					{
						channelActive = false;
						//continue; //TODO: Why does this break things?
					}
					else
					{
						channelActive = true;
					}
				}
				else
				{
					channelActive = true;
				}

				assert(animation.samplers.size() > channel.sampler);
				AnimationSampler& animSampler = animation.samplers[channel.sampler];
				assert(animSampler.interpolation == "LINEAR");

				assert(model.accessors.size() > animSampler.input);
				Accessor& timeAccessor = model.accessors[animSampler.input];
				assert(timeAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(timeAccessor.type == TINYGLTF_TYPE_SCALAR);

				const float* times = ReadBuffer<float>(model, timeAccessor);

				const XMFLOAT3* translations = nullptr;
				const XMVECTOR* rotations = nullptr;
				const XMFLOAT3* scales = nullptr;

				AnimationData* animData = nullptr;
				AnimationJointData& animJointData = transformAnimation.jointChannels[hierachy->nodeToJointIndex[channel.target_node]];

				if (channel.target_path == "translation")
				{
					assert(model.accessors.size() > animSampler.output);
					Accessor& valueAccessor = model.accessors[animSampler.output];
					assert(valueAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
					assert(valueAccessor.type == TINYGLTF_TYPE_VEC3);
					translations = ReadBuffer<XMFLOAT3>(model, valueAccessor);
					animData = &animJointData.translations;
				}
				else if (channel.target_path == "rotation")
				{
					assert(model.accessors.size() > animSampler.output);
					Accessor& valueAccessor = model.accessors[animSampler.output];
					assert(valueAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
					assert(valueAccessor.type == TINYGLTF_TYPE_VEC4);
					rotations = ReadBuffer<XMVECTOR>(model, valueAccessor);
					animData = &animJointData.rotations;
				}
				else if (channel.target_path == "scale")
				{
					assert(model.accessors.size() > animSampler.output);
					Accessor& valueAccessor = model.accessors[animSampler.output];
					assert(valueAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
					assert(valueAccessor.type == TINYGLTF_TYPE_VEC3);
					scales = ReadBuffer<XMFLOAT3>(model, valueAccessor);
					animData = &animJointData.scales;
				}
				else
				{
					WARN("Unknown channel target path!");
					continue;
				}

				animData->times = NewArray(arena, float, timeAccessor.count);
				animData->data = NewArray(arena, XMVECTOR, timeAccessor.count);

				// TODO: resample animation?
				for (int i = 0; i < timeAccessor.count; i++)
				{
					if (translations != nullptr)
					{
						animData->data[animData->frameCount] = XMLoadFloat3(&translations[i]);
					}
					else if (rotations != nullptr)
					{
						animData->data[animData->frameCount] = rotations[i];
					}
					else if (scales != nullptr)
					{
						animData->data[animData->frameCount] = XMLoadFloat3(&scales[i]);
					}
					else
					{
						assert(false);
					}

					animData->times[animData->frameCount] = times[i];
					animData->frameCount++;

					maxTime = std::max(maxTime, times[i]);
				}
			}

			transformAnimation.duration = maxTime;
		}

		LOG_TIMER(timer, "Animations");
	}
	
	RESET_TIMER(timer);

	std::vector<MeshFile> meshFiles{};

	for (Mesh& mesh : model.meshes)
	{
		for (Primitive& primitive : mesh.primitives)
		{
			Accessor& indexAccessor = model.accessors[primitive.indices];
			assert(indexAccessor.type == TINYGLTF_TYPE_SCALAR);

			indices = NewArray(arena, INDEX_BUFFER_TYPE, indexAccessor.count);
			indexCount = indexAccessor.count;
			
			if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				const uint16_t* indexData = ReadBuffer<uint16_t>(model, indexAccessor);
				for (int i = 0; i < indexCount; i++)
				{
					indices[i] = indexData[i];
				}
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			{
				const uint32_t* indexData = ReadBuffer<uint32_t>(model, indexAccessor);
				memcpy_s(indices, sizeof(INDEX_BUFFER_TYPE)* indexCount, indexData, sizeof(INDEX_BUFFER_TYPE)* indexAccessor.count);
			}
			else
			{
				assert(false);
			}

			Accessor& positionAccessor = CheckAccessor(model, primitive, GLTF_POSITION, TINYGLTF_TYPE_VEC3);
			const float* positionData = ReadBuffer<float>(model, positionAccessor);
			Accessor& normalAccessor = CheckAccessor(model, primitive, GLTF_NORMAL, TINYGLTF_TYPE_VEC3);
			const float* normalData = ReadBuffer<float>(model, normalAccessor);
			Accessor& tangentAccessor = CheckAccessor(model, primitive, GLTF_TANGENT, TINYGLTF_TYPE_VEC4);
			const float* tangentData = ReadBuffer<float>(model, tangentAccessor);
			Accessor& uvAccessor = CheckAccessor(model, primitive, GLTF_TEXCOORD0, TINYGLTF_TYPE_VEC2);
			const float* uvData = ReadBuffer<float>(model, uvAccessor);

			vertices = NewArray(arena, Vertex, positionAccessor.count);
			vertexCount = positionAccessor.count;

			for (int i = 0; i < vertexCount; i++)
			{
				assert(i < positionAccessor.count);
				assert(i < normalAccessor.count);
				assert(i < tangentAccessor.count);
				assert(i < uvAccessor.count);
				Vertex& vert = vertices[i];

				vert.position.x = positionData[i * 3 + 0];
				vert.position.y = positionData[i * 3 + 1];
				vert.position.z = positionData[i * 3 + 2];

				vert.normal.x = normalData[i * 3 + 0];
				vert.normal.y = normalData[i * 3 + 1];
				vert.normal.z = normalData[i * 3 + 2];

				vert.tangent.x = tangentData[i * 4 + 0];
				vert.tangent.y = tangentData[i * 4 + 1];
				vert.tangent.z = tangentData[i * 4 + 2];

				XMVECTOR bitangent = XMVectorScale(XMVector3Cross(XMLoadFloat3(&vert.normal), XMLoadFloat3(&vert.tangent)), tangentData[i * 4 + 3]);
				XMStoreFloat3(&vert.bitangent, bitangent);

				vert.uv.x = uvData[i * 2 + 0];
				vert.uv.y = uvData[i * 2 + 1];
			}

			if (hierachy != nullptr)
			{
				Accessor& jointAccessor = model.accessors[primitive.attributes[GLTF_JOINTS]];
				assert(jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
				assert(jointAccessor.type == TINYGLTF_TYPE_VEC4);
				const uint8_t* jointData = ReadBuffer<uint8_t>(model, jointAccessor);

				Accessor& weightAccessor = model.accessors[primitive.attributes[GLTF_WEIGHTS]];
				assert(weightAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(weightAccessor.type == TINYGLTF_TYPE_VEC4);
				const float* weightData = ReadBuffer<float>(model, weightAccessor);

				for (int i = 0; i < vertexCount; i++)
				{
					assert(i < jointAccessor.count);
					assert(i < weightAccessor.count);
					Vertex& vert = vertices[i];

					vert.boneIndices.x = jointData[i * 4 + 0];
					vert.boneIndices.y = jointData[i * 4 + 1];
					vert.boneIndices.z = jointData[i * 4 + 2];
					vert.boneIndices.w = jointData[i * 4 + 3];

					float w0 = weightData[i * 4 + 0];
					float w1 = weightData[i * 4 + 1];
					float w2 = weightData[i * 4 + 2];
					float w3 = weightData[i * 4 + 3];
					assert(abs(1. - (w0 + w1 + w2 + w3)) < 0.01);

					vert.boneWeights.x = w0;
					vert.boneWeights.y = w1;
					vert.boneWeights.z = w2;
					vert.boneWeights.w = w3;
				}
			}

			std::string materialName{};
			size_t textureCount = 1; // TODO

			if (model.materials.size() > 0)
			{
				assert(model.materials.size() > primitive.material);
				materialName = model.materials[primitive.material].name;

				if (model.materials[primitive.material].extras.Has("texturecount"))
				{
					textureCount = model.materials[primitive.material].extras.Get("texturecount").GetNumberAsInt();
				}
			}

			meshFiles.emplace_back(MeshFile{ vertices, vertexCount, indices, indexCount, materialName, textureCount });
		}
	}

	LOG_TIMER(timer, "Meshes");
	RESET_TIMER(timer);
	
	return { meshFiles, hierachy };
}

void TransformHierachy::UpdateNode(TransformNode* node)
{
	assert(node != nullptr);
	if (node->parent != nullptr)
	{
		node->global = XMMatrixMultiply(node->currentLocal, node->parent->global);
	}
	else
	{
		node->global = node->currentLocal;
	}

	for (int i = 0; i < node->childCount; i++)
	{
		UpdateNode(node->children[i]);
	}
}

void TransformHierachy::SetAnimationActive(std::string name, bool state)
{
	animations[animationNameToIndex[name]].active = state;
}