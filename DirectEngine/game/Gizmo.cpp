#include "Game.h"

Gizmo* LoadGizmo(EngineCore& engine, Game& game, size_t materialIndex)
{
	Gizmo* gizmo = NewObject(game.globalArena, Gizmo);

	MeshFile translateArrowMeshFile = LoadGltfFromFile("models/translate-arrow.glb", game.debugLog, game.modelArena).meshes[0];
	gizmo->translateArrowMesh = engine.CreateMesh(materialIndex, translateArrowMeshFile.vertices, translateArrowMeshFile.vertexCount);

	MeshFile rotateArrowMeshFile = LoadGltfFromFile("models/rotate-arrow.glb", game.debugLog, game.modelArena).meshes[0];
	gizmo->rotateArrowMesh = engine.CreateMesh(materialIndex, rotateArrowMeshFile.vertices, rotateArrowMeshFile.vertexCount);

	MeshFile scaleArrowMeshFile = LoadGltfFromFile("models/scale-arrow.glb", game.debugLog, game.modelArena).meshes[0];
	gizmo->scaleArrowMesh = engine.CreateMesh(materialIndex, scaleArrowMeshFile.vertices, scaleArrowMeshFile.vertexCount);

	XMVECTOR xyzColors[3] = {
		{ 1.f, 0.f, 0.f, 1.f },
		{ 0.f, 1.f, 0.f, 1.f },
		{ 0.f, 0.f, 1.f, 1.f },
	};

	XMVECTOR xyzRotations[3] = {
		XMQuaternionRotationRollPitchYaw(0.f, 0.f, -XM_PIDIV2),
		XMQuaternionRotationRollPitchYaw(0.f, 0.f, 0.f),
		XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0.f, 0.f),
	};

	XMVECTOR transformAxis[3] = {
		{ 1.f, 0.f, 0.f },
		{ 0.f, 1.f, 0.f },
		{ 0.f, 0.f, 1.f },
	};

	const char* xyzNames[3] = {
		"X",
		"Y",
		"Z",
	};

	gizmo->root = game.CreateEmptyEntity(engine);
	gizmo->root->SetActive(false);
	gizmo->root->name = "Gizmo";

	for (int i = 0; i < 3; i++)
	{
		Entity* translateArrow = gizmo->translateArrows[i];
		translateArrow = game.CreateMeshEntity(engine, materialIndex, gizmo->translateArrowMesh);
		translateArrow->GetBuffer().color = xyzColors[i];
		translateArrow->collisionData->aabbLocalSize = { 0.1f, 1.f, 0.1f };
		translateArrow->collisionData->aabbLocalPosition = { 0.f, .5f, 0.f };
		translateArrow->collisionData->collisionLayers = CollisionLayers::GizmoClick;
		translateArrow->rotation = xyzRotations[i];
		translateArrow->name = std::format("TranslateArrow{}", xyzNames[i]).c_str();
		translateArrow->isGizmoTranslationArrow = true;
		translateArrow->gizmoTranslationAxis = transformAxis[i];
		gizmo->root->AddChild(translateArrow);

		Entity* rotateArrow = gizmo->rotateArrows[i];
		rotateArrow = game.CreateMeshEntity(engine, materialIndex, gizmo->rotateArrowMesh);
		rotateArrow->GetBuffer().color = xyzColors[i];
		rotateArrow->collisionData->aabbLocalSize = { 2.f, .05f, 2.f };
		rotateArrow->collisionData->aabbLocalPosition = { 0.f, 0.f, 0.f };
		rotateArrow->collisionData->collisionLayers = CollisionLayers::GizmoClick;
		rotateArrow->rotation = xyzRotations[i];
		rotateArrow->name = std::format("RotateArrow{}", xyzNames[i]).c_str();
		rotateArrow->isGizmoRotationRing = true;
		rotateArrow->gizmoRotationAxis = transformAxis[i];
		gizmo->root->AddChild(rotateArrow);

		Entity* scaleArrow = gizmo->scaleArrows[i];
		scaleArrow = game.CreateMeshEntity(engine, materialIndex, gizmo->scaleArrowMesh);
		scaleArrow->GetBuffer().color = xyzColors[i];
		scaleArrow->collisionData->aabbLocalSize = { .15f, .5f, .15f };
		scaleArrow->collisionData->aabbLocalPosition = { .0f, .25f, .0f };
		scaleArrow->collisionData->collisionLayers = CollisionLayers::GizmoClick;
		scaleArrow->rotation = xyzRotations[i];
		scaleArrow->name = std::format("ScaleArrow{}", xyzNames[i]).c_str();
		scaleArrow->isGizmoScaleCube = true;
		scaleArrow->gizmoScaleAxis = transformAxis[i];
		gizmo->root->AddChild(scaleArrow);
	}

	return gizmo;
}