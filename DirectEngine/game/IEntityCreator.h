#pragma once

#include "Entity.h"

class IEntityCreator
{
public:
	virtual Entity* CreateEmptyEntity(EngineCore& engine) = 0;
	virtual Entity* CreateMeshEntity(EngineCore& engine, MaterialData* material, MeshDataGPU* meshData) = 0;
	virtual Entity* CreateQuadEntity(EngineCore& engine, MaterialData* material, float width, float height, bool vertical = false) = 0;
	virtual Entity* CreateQuadEntity(EngineCore& engine, MaterialData* material, float width, float height, PhysicsInit& physicsInit, bool vertical = false) = 0;
	virtual Entity* CreateEntityFromGltf(EngineCore& engine, const char* path) = 0;
};