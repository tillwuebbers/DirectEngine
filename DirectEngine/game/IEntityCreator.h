#pragma once

#include "Assets.h"
#include "Entity.h"

class IEntityCreator
{
public:
	virtual Entity* CreateEmptyEntity(EngineCore& engine) = 0;
	virtual Entity* CreateMeshEntity(EngineCore& engine, size_t drawCallIndex, MeshData* meshData) = 0;
	virtual Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, bool vertical = false) = 0;
	virtual Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, PhysicsInit& physicsInit, bool vertical = false) = 0;
	virtual Entity* CreateEntityFromGltf(EngineCore& engine, const char* path, Assets::Shader shader) = 0;
};