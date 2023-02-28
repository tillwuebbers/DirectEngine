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
		gizmo->translateArrows[i] = game.CreateMeshEntity(engine, materialIndex, gizmo->translateArrowMesh);
		gizmo->translateArrows[i]->GetBuffer().color = xyzColors[i];
		gizmo->translateArrows[i]->collisionData->aabbLocalSize = { 0.1f, 1.f, 0.1f };
		gizmo->translateArrows[i]->collisionData->aabbLocalPosition = { 0.f, .5f, 0.f };
		gizmo->translateArrows[i]->rotation = xyzRotations[i];
		gizmo->translateArrows[i]->name = std::format("TranslateArrow{}", xyzNames[i]).c_str();
		gizmo->root->AddChild(gizmo->translateArrows[i]);

		gizmo->rotateArrows[i] = game.CreateMeshEntity(engine, materialIndex, gizmo->rotateArrowMesh);
		gizmo->rotateArrows[i]->GetBuffer().color = xyzColors[i];
		gizmo->rotateArrows[i]->collisionData->aabbLocalSize = { 2.f, .05f, 2.f };
		gizmo->rotateArrows[i]->collisionData->aabbLocalPosition = { 0.f, 0.f, 0.f };
		gizmo->rotateArrows[i]->rotation = xyzRotations[i];
		gizmo->rotateArrows[i]->name = std::format("RotateArrow{}", xyzNames[i]).c_str();
		gizmo->root->AddChild(gizmo->rotateArrows[i]);

		gizmo->scaleArrows[i] = game.CreateMeshEntity(engine, materialIndex, gizmo->scaleArrowMesh);
		gizmo->scaleArrows[i]->GetBuffer().color = xyzColors[i];
		gizmo->scaleArrows[i]->collisionData->aabbLocalSize = { .08f, .5f, .08f };
		gizmo->scaleArrows[i]->collisionData->aabbLocalPosition = { .0f, .25f, .0f };
		gizmo->scaleArrows[i]->rotation = xyzRotations[i];
		gizmo->scaleArrows[i]->name = std::format("ScaleArrow{}", xyzNames[i]).c_str();
		gizmo->root->AddChild(gizmo->scaleArrows[i]);
	}

	return gizmo;
}