#pragma once

#include "Log.h"

#include <string>
#include <vector>
#include <DirectXMath.h>
using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
	int entityID;
};

struct MeshFile
{
	std::vector<Vertex> vertices;
};

void CreateQuad(MeshFile& meshFileOut, float width, float height);
void LoadMeshFromFile(MeshFile& meshFileOut, const std::string& filePath, const std::string& materialPath, RingLog& debugLog);