#pragma once

#include "Entity.h"

class IEntityCreator
{
public:
	virtual Entity* CreateEmptyEntity(EngineCore& engine) = 0;
	virtual Entity* CreateMeshEntity(EngineCore& engine, size_t drawCallIndex, D3D12_VERTEX_BUFFER_VIEW& meshView) = 0;
	virtual Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, bool vertical = false) = 0;
	virtual Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, PhysicsInit& physicsInit, bool vertical = false) = 0;
	virtual Entity* CreateEntityFromGltf(EngineCore& engine, const char* path, const std::wstring& shaderName) = 0;
};