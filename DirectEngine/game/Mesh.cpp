#include "Mesh.h"

#include <format>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../import/tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "../import/tiny_gltf.h"
using namespace tinygltf;

MeshFile CreateQuad(float width, float height, MemoryArena& vertexArena, MemoryArena& indexArena)
{
	const size_t VERTEX_COUNT = 6;

	Vertex* vertices = NewArray(vertexArena, Vertex, VERTEX_COUNT);
	vertices[0] = { { 0.f  , 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {0.f, 0.f}, 0, 0 };
	vertices[1] = { { width, 0.f, height }, {}, {0.f, 1.f, 0.f }, {1.f, 1.f}, 0, 0 };
	vertices[2] = { { width, 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {1.f, 0.f}, 0, 0 };
	vertices[3] = { { 0.f  , 0.f, 0.f    }, {}, {0.f, 1.f, 0.f }, {0.f, 0.f}, 0, 0 };
	vertices[4] = { { 0.f  , 0.f, height }, {}, {0.f, 1.f, 0.f }, {0.f, 1.f}, 0, 0 };
	vertices[5] = { { width, 0.f, height }, {}, {0.f, 1.f, 0.f }, {1.f, 1.f}, 0, 0 };

	return MeshFile{ vertices, VERTEX_COUNT, nullptr, 0 };
}

MeshFile LoadObjFromFile(const std::string& filePath, const std::string& materialPath, RingLog& debugLog, MemoryArena& vertexArena, MemoryArena& indexArena)
{
	tinyobj::ObjReaderConfig reader_config;
	reader_config.mtl_search_path = materialPath;
	tinyobj::ObjReader reader;

	if (!reader.ParseFromFile(filePath, reader_config))
	{
		if (!reader.Error().empty())
		{
			debugLog.Error(std::format("OBJ {} error: {}", filePath, reader.Error()));
		}
	}

	if (!reader.Warning().empty())
	{
		debugLog.Warn(std::format("OBJ {} warning: {}", filePath, reader.Warning()));
	}

	auto& attrib = reader.GetAttrib();
	auto& shapes = reader.GetShapes();
	auto& materials = reader.GetMaterials();

	const uint64_t colorCount = attrib.colors.size() / 3;
	const uint64_t normalCount = attrib.colors.size() / 3;
	const uint64_t texcoordCount = attrib.colors.size() / 2;
	const uint64_t vertexCount = attrib.vertices.size() / 3;
	const uint64_t indexCount = reader.GetShapes()[0].mesh.indices.size();

	debugLog.Log(std::format("Loaded OBJ {}: {} shapes, {} materials", filePath, shapes.size(), materials.size()));
	debugLog.Log(std::format("{} colors, {} normals, {} texcoords, {} vertices", colorCount, normalCount, texcoordCount, vertexCount));

	Vertex* vertices = NewArray(vertexArena, Vertex, indexCount);
	size_t vertexCounter = 0;

	// Loop over shapes
	for (size_t shapeIdx = 0; shapeIdx < shapes.size(); shapeIdx++)
	{
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t faceIdx = 0; faceIdx < shapes[shapeIdx].mesh.num_face_vertices.size(); faceIdx++)
		{
			size_t fv = size_t(shapes[shapeIdx].mesh.num_face_vertices[faceIdx]);

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++)
			{
				// access to vertex
				tinyobj::index_t idx = shapes[shapeIdx].mesh.indices[index_offset + v];
				assert(vertexCounter < indexCount);
				Vertex& vert = vertices[vertexCounter];
				vertexCounter++;

				vert.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
				vert.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
				vert.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

				if (idx.normal_index >= 0) {
					vert.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
					vert.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
					vert.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
				}

				if (idx.texcoord_index >= 0) {
					vert.uv.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
					vert.uv.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
				}

				vert.color.x = attrib.colors[3 * size_t(idx.vertex_index) + 0];
				vert.color.y = attrib.colors[3 * size_t(idx.vertex_index) + 1];
				vert.color.z = attrib.colors[3 * size_t(idx.vertex_index) + 2];
				vert.color.w = 1.0f;

				//uint8_t w0 = attrib.skin_weights[];

				//vertices[idx.vertex_index] = vert;
				//indices.push_back(idx.vertex_index);
			}
			index_offset += fv;

			// per-face material
			//shapes[shapeIdx].mesh.material_ids[faceIdx];
		}
	}

	return MeshFile{ vertices, indexCount, nullptr, 0 };
}

template <typename T>
const T* ReadBuffer(Model& model, Accessor& accessor)
{
	BufferView& bufferView = model.bufferViews[accessor.bufferView];
	Buffer& buffer = model.buffers[bufferView.buffer];
	return reinterpret_cast<T*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
}

MeshFile LoadGltfFromFile(const std::string& filePath, RingLog& debugLog, MemoryArena& vertexArena)
{

	Model model;
	TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);

	if (!warn.empty()) {
		OutputDebugStringA(std::format("Warn: {}\n", warn).c_str());
	}

	if (!err.empty()) {
		OutputDebugStringA(std::format("Err: {}\n", err).c_str());
	}

	if (!ret) {
		OutputDebugStringA(std::format("Failed to parse glTF\n").c_str());
	}

	Vertex* vertices;
	size_t vertexCount = 0;
	for (Mesh& mesh : model.meshes)
	{
		// TODO: support multiple primitives
		//for (Primitive& primitive : mesh.primitives)
		Primitive& primitive = mesh.primitives[0];
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

				vert.boneIndices = 0;
				vert.boneIndices |= jointData[idx * 4 + 0];
				vert.boneIndices |= jointData[idx * 4 + 1] << 8;
				vert.boneIndices |= jointData[idx * 4 + 2] << 16;
				vert.boneIndices |= jointData[idx * 4 + 3] << 24;

				float w0 = weightData[idx * 4 + 0];
				float w1 = weightData[idx * 4 + 1];
				float w2 = weightData[idx * 4 + 2];
				float w3 = weightData[idx * 4 + 3];
				assert(abs(1. - (w0 + w1 + w2 + w3)) < 0.01);

				vert.boneWeights = 0;
				vert.boneWeights |= static_cast<uint8_t>(w0 * 255.);
				vert.boneWeights |= static_cast<uint8_t>(w1 * 255.) << 8;
				vert.boneWeights |= static_cast<uint8_t>(w2 * 255.) << 16;
				vert.boneWeights |= static_cast<uint8_t>(w3 * 255.) << 24;
			}
		}
	}

	return MeshFile{ vertices, vertexCount, nullptr, 0 };
}