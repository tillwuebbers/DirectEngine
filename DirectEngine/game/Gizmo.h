#pragma once

#include "IEntityCreator.h"
#include "Entity.h"

struct Gizmo
{
	Entity* root;
	Entity* translateArrows[3];
	Entity* rotateArrows[3];
	Entity* scaleArrows[3];

	D3D12_VERTEX_BUFFER_VIEW translateArrowMesh;
	D3D12_VERTEX_BUFFER_VIEW rotateArrowMesh;
	D3D12_VERTEX_BUFFER_VIEW scaleArrowMesh;

	bool gizmoLocal = true;
	bool editMode = false;
	Entity* editElement = nullptr;
	Entity* selectedGizmoElement = nullptr;
	Entity* selectedGizmoTarget = nullptr;
	XMVECTOR gizmoDragCursorStart{};
	XMVECTOR gizmoDragEntityStart{};

	void Init(MemoryArena& arena, IEntityCreator* game, EngineCore& engine, size_t materialIndex);
	void Update(EngineInput& input);
};
