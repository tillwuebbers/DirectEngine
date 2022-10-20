#include "Game.h"

#include <array>
#include <format>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../import/tiny_obj_loader.h"

Game::Game(std::wstring name) :
	name(name),
	debugLog{},
	showLog(true)
{}

void Game::StartGame()
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

	// Loop over shapes
	for (size_t shapeIdx = 0; shapeIdx < shapes.size(); shapeIdx++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t faceIdx = 0; faceIdx < shapes[shapeIdx].mesh.num_face_vertices.size(); faceIdx++) {
			size_t fv = size_t(shapes[shapeIdx].mesh.num_face_vertices[faceIdx]);

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[shapeIdx].mesh.indices[index_offset + v];
				Vertex vert{};

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
			shapes[shapeIdx].mesh.material_ids[faceIdx];
		}
	}
}

void Game::UpdateGame()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{ 100, 30 });
	if (showLog)
	{
		ImGui::Begin("Log", &showLog);

		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
		for (LogMessage& message : debugLog)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, message.color.Value);
			ImGui::Text(message.message.c_str());
			ImGui::PopStyleColor();
		}
		ImGui::PopStyleVar();
		ImGui::EndChild();

		ImGui::End();
	}
	ImGui::PopStyleVar();
}

void Game::Log(const char* message)
{
	Log(std::string(message));
}

void Game::Log(std::string message)
{
	LogMessage msg{};
	msg.color = IM_COL32_WHITE;
	msg.message = message;
	debugLog.push_back(msg);
}

void Game::Warn(const char* message)
{
	Warn(std::string(message));
}

void Game::Warn(std::string message)
{
	LogMessage msg{};
	msg.color = ImColor::HSV(.09f, .9f, 1.f);
	msg.message = message;
	debugLog.push_back(msg);
}

void Game::Error(const char* message)
{
	Error(std::string(message));
}

void Game::Error(std::string message)
{
	LogMessage msg{};
	msg.color = ImColor::HSV(.0f, .9f, 1.f);
	msg.message = message;
	debugLog.push_back(msg);
}