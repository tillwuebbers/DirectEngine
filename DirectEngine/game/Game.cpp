#include "Game.h"

#include <array>
#include <format>

#include "../core/vkcodes.h"
#include "remixicon.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../import/tiny_obj_loader.h"

Game::Game(std::wstring name) : name(name)
{}

void Game::StartGame(EngineCore& engine)
{
	const char* filePath = "cube.obj";

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

				vert.color.x = attrib.colors[3*size_t(idx.vertex_index)+0];
				vert.color.y = attrib.colors[3*size_t(idx.vertex_index)+1];
				vert.color.z = attrib.colors[3*size_t(idx.vertex_index)+2];
				vert.color.w = 1.0f;

				vertices[idx.vertex_index] = vert;
				indices.push_back(idx.vertex_index);
			}
			index_offset += fv;

			// per-face material
			//shapes[shapeIdx].mesh.material_ids[faceIdx];
		}
	}

	engine.CreateMesh(copiedVertices.data(), sizeof(Vertex), copiedVertices.size(), 100);
}

void Game::UpdateGame(EngineCore& engine)
{
	engine.BeginProfile("Input Mutex", ImColor::HSV(.5f, 1.f, .5f));
	if (input.KeyJustPressed(VK_KEY_R))
	{
		engine.m_startTime = std::chrono::high_resolution_clock::now();
	}
	input.NextFrame();
	engine.EndProfile("Input Mutex");

	engine.BeginProfile("Game Update", ImColor::HSV(.75f, 1.f, 1.f));

	engine.m_constantBufferData.modelTransform = DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationY(engine.TimeSinceStart()));

	XMVECTOR pos{ 0.f, 0.f, -10.f };
	XMVECTOR lookAt{ 0.f, 0.f, 0.f };
	XMVECTOR up{ 0.f, 1.f, 0.f };
	engine.m_constantBufferData.cameraTransform = DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(pos, lookAt, up));
	engine.m_constantBufferData.clipTransform = DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(45.f, engine.m_aspectRatio, .1f, 1000.f));

	engine.EndProfile("Game Update");

	engine.BeginProfile("Game UI", ImColor::HSV(.75f, 1.f, .75f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{ 100, 30 });
	if (showLog)
	{
		if (ImGui::Begin("Log", &showLog))
		{
			if (ImGui::Button(stopLog ? ICON_PLAY_FILL : ICON_STOP_FILL))
			{
				if (!stopLog)
				{
					Warn("Log paused...");
				}
				stopLog = !stopLog;
				if (!stopLog)
				{
					Warn("Log resumed...");
				}
			}
			ImGui::SameLine();

			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
			debugLog.DrawText();
			ImGui::PopStyleVar();
			ImGui::EndChild();
		}
		ImGui::End();
	}
	ImGui::PopStyleVar();
	engine.EndProfile("Game UI");
}

EngineInput& Game::GetInput()
{
	return input;
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