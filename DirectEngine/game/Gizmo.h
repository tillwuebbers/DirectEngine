#pragma once

#include "IEntityCreator.h"
#include "Entity.h"

struct Gizmo
{
	Entity* root;
	Entity* translateArrows[3];
	Entity* rotateArrows[3];
	Entity* scaleArrows[3];

	MeshDataGPU* translateArrowMesh;
	MeshDataGPU* rotateArrowMesh;
	MeshDataGPU* scaleArrowMesh;

	bool gizmoLocal = true;
	bool editMode = false;
	Entity* editElement = nullptr;
	Entity* selectedGizmoElement = nullptr;
	Entity* selectedGizmoTarget = nullptr;
	XMVECTOR gizmoDragCursorStart{};
	XMVECTOR gizmoDragEntityStart{};

	void Init(MemoryArena& arena, IEntityCreator* game, EngineCore& engine, MaterialData* material);
	void Update(EngineInput& input);
};
