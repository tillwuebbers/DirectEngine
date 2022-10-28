#include "Game.h"

#include <array>
#include <format>

#include "../core/vkcodes.h"
#include "remixicon.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../import/tiny_obj_loader.h"

void CreateWorldMatrix(XMMATRIX& out, const XMFLOAT3& scale, const XMFLOAT4& rotation, const XMFLOAT3& position)
{
	out = DirectX::XMMatrixTranspose(DirectX::XMMatrixAffineTransformation(
		XMLoadFloat3(&scale), XMVECTOR{}, XMLoadFloat4(&rotation), XMLoadFloat3(&position)));
}

Game::Game()
{}

void Game::StartGame(EngineCore& engine)
{
	size_t cubeMeshIndex = LoadMeshFromFile(engine, "cube.obj");

	Entity* entity1 = CreateEntity(engine, cubeMeshIndex);
	Entity* entity2 = CreateEntity(engine, cubeMeshIndex);
	entity2->position = XMFLOAT3{ 0.f, 0.f, 5.f };

	camera.position = { 0.f, 0.f, 10.f };
}

void Game::UpdateGame(EngineCore& engine)
{
	engine.BeginProfile("Input", ImColor::HSV(.5f, 1.f, .5f));
	if (input.KeyJustPressed(VK_ESCAPE))
	{
		showEscMenu = !showEscMenu;
	}
	if (input.KeyJustPressed(VK_KEY_R))
	{
		engine.m_startTime = std::chrono::high_resolution_clock::now();
	}
	if (input.KeyDown(VK_KEY_A))
	{
		camera.position.x -= engine.m_updateDeltaTime * 10.f;
	}
	if (input.KeyDown(VK_KEY_D))
	{
		camera.position.x += engine.m_updateDeltaTime * 10.f;
	}
	if (input.KeyJustPressed(VK_KEY_D, VK_CONTROL))
	{
		showDebugUI = !showDebugUI;
	}
	if (input.KeyJustPressed(VK_KEY_L, VK_CONTROL))
	{
		showLog = !showLog;
	}
	if (input.KeyJustPressed(VK_KEY_P, VK_CONTROL))
	{
		showProfiler = !showProfiler;
	}
	input.NextFrame();
	engine.EndProfile("Input");

	engine.BeginProfile("Game Update", ImColor::HSV(.75f, 1.f, 1.f));
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		EntityData& data = engine.m_entities[entity->dataIndex];

		entity->position.x = static_cast<float>(sin(engine.TimeSinceStart()));
		XMStoreFloat4(&entity->rotation, XMQuaternionRotationAxis(XMVECTOR{0.f, 1.f, 0.f}, static_cast<float>(engine.TimeSinceStart())));

		CreateWorldMatrix(data.constantBufferData.worldTransform, entity->scale, entity->rotation, entity->position);

		// TODO: Race condition?
		memcpy(data.mappedConstantBufferData, &data.constantBufferData, sizeof(data.constantBufferData));
	}

	XMFLOAT3 cameraScale = XMFLOAT3{ 1.f, 1.f, 1.f };

	CreateWorldMatrix(engine.m_constantBufferData.cameraTransform, cameraScale, camera.rotation, camera.position);

	engine.m_constantBufferData.clipTransform = XMMatrixTranspose(XMMatrixPerspectiveFovLH(45.f, engine.m_aspectRatio, .1f, 1000.f));

	engine.EndProfile("Game Update");

	if (!pauseProfiler)
	{
		lastDebugFrameIndex = (lastDebugFrameIndex + 1) % _countof(lastFrames);
		lastFrames[lastDebugFrameIndex].duration = static_cast<float>(engine.m_updateDeltaTime);
	}

	DrawUI(engine);
}

EngineInput& Game::GetInput()
{
	return input;
}

size_t Game::LoadMeshFromFile(EngineCore& engine, const std::string& filePath)
{
	tinyobj::ObjReaderConfig reader_config;
	reader_config.mtl_search_path = "./";
	tinyobj::ObjReader reader;

	if (!reader.ParseFromFile(filePath, reader_config))
	{
		if (!reader.Error().empty())
		{
			Error(std::format("OBJ {} error: {}", filePath, reader.Error()));
		}
	}

	if (!reader.Warning().empty())
	{
		Warn(std::format("OBJ {} warning: {}", filePath, reader.Warning()));
	}

	auto& attrib = reader.GetAttrib();
	auto& shapes = reader.GetShapes();
	auto& materials = reader.GetMaterials();

	const uint64_t colorCount = attrib.colors.size() / 3;
	const uint64_t normalCount = attrib.colors.size() / 3;
	const uint64_t texcoordCount = attrib.colors.size() / 2;
	const uint64_t vertexCount = attrib.vertices.size() / 3;

	Log(std::format("Loaded OBJ {}: {} shapes, {} materials", filePath, shapes.size(), materials.size()));
	Log(std::format("{} colors, {} normals, {} texcoords, {} vertices", colorCount, normalCount, texcoordCount, vertexCount));

	std::vector<Vertex> vertices{ vertexCount };
	std::vector<uint64_t> indices{};
	std::vector<Vertex> copiedVertices{};

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
				Vertex& vert = copiedVertices.emplace_back();

				vert.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
				vert.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
				vert.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

				if (idx.normal_index >= 0) {
					tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
					tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
					tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
				}

				if (idx.texcoord_index >= 0) {
					tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
					tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
				}

				vert.color.x = attrib.colors[3 * size_t(idx.vertex_index) + 0];
				vert.color.y = attrib.colors[3 * size_t(idx.vertex_index) + 1];
				vert.color.z = attrib.colors[3 * size_t(idx.vertex_index) + 2];
				vert.color.w = 1.0f;

				vertices[idx.vertex_index] = vert;
				indices.push_back(idx.vertex_index);
			}
			index_offset += fv;

			// per-face material
			//shapes[shapeIdx].mesh.material_ids[faceIdx];
		}
	}

	return engine.CreateMesh(copiedVertices.data(), sizeof(Vertex), copiedVertices.size(), 100);
}

Entity* Game::CreateEntity(EngineCore& engine, size_t meshIndex)
{
	Entity* entity = NewObject(entityArena, Entity);
	
	EntityData& data = engine.m_entities.emplace_back();
	entity->dataIndex = engine.m_entities.size() - 1;
	data.meshIndex = meshIndex;

	MeshData& mesh = engine.m_meshes[meshIndex];
	engine.CreateConstantBuffer<EntityConstantBuffer>(data.constantBuffer, &data.constantBufferData, &data.mappedConstantBufferData);
	data.constantBuffer->SetName(std::format(L"Entity Constant Buffer {}", meshIndex).c_str());
	return entity;
}

void Game::Log(std::string message)
{
	if (!stopLog) debugLog.AppendToLog(message, IM_COL32_WHITE);
}

void Game::Warn(std::string message)
{
	if (!stopLog) debugLog.AppendToLog(message, ImColor::HSV(.09f, .9f, 1.f));
}

void Game::Error(std::string message)
{
	if (!stopLog) debugLog.AppendToLog(message, ImColor::HSV(.0f, .9f, 1.f));
}

void RingLog::AppendToLog(std::string message, ImU32 color)
{
	messages[debugLogIndex].color = color;
	messages[debugLogIndex].message = message;
	debugLogIndex = (debugLogIndex + 1) % LOG_SIZE;

	if (debugLogCount < LOG_SIZE)
	{
		debugLogCount++;
	}
}

void RingLog::DrawText()
{
	for (int i = 0; i < LOG_SIZE; i++)
	{
		int realIndex = (debugLogIndex - debugLogCount + i + LOG_SIZE) % LOG_SIZE;
		LogMessage& message = messages[realIndex];
		ImGui::PushStyleColor(ImGuiCol_Text, message.color.Value);
		ImGui::Text(message.message.c_str());
		ImGui::PopStyleColor();
	}
}