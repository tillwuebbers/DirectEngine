#pragma once

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
};

struct MeshFile
{
	Vertex* vertices;
	size_t vertexCount;
	uint64_t* indices;
	size_t indexCount;
};

MeshFile CreateQuad(float width, float height, MemoryArena& vertexArena, MemoryArena& indexArena);
MeshFile LoadMeshFromFile(const std::string& filePath, const std::string& materialPath, RingLog& debugLog, MemoryArena& vertexArena, MemoryArena& indexArena);