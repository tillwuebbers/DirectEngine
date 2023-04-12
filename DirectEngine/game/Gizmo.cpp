#include "Game.h"

Gizmo* LoadGizmo(EngineCore& engine, Game& game, size_t materialIndex)
{
	Gizmo* gizmo = NewObject(game.globalArena, Gizmo);

	MeshFile translateArrowMeshFile = LoadGltfFromFile("models/translate-arrow.glb", game.debugLog, game.globalArena).meshes[0];
	gizmo->translateArrowMesh = engine.CreateMesh(materialIndex, translateArrowMeshFile.vertices, translateArrowMeshFile.vertexCount, translateArrowMeshFile.meshHash);

	MeshFile rotateArrowMeshFile = LoadGltfFromFile("models/rotate-arrow.glb", game.debugLog, game.globalArena).meshes[0];
	gizmo->rotateArrowMesh = engine.CreateMesh(materialIndex, rotateArrowMeshFile.vertices, rotateArrowMeshFile.vertexCount, rotateArrowMeshFile.meshHash);

	MeshFile scaleArrowMeshFile = LoadGltfFromFile("models/scale-arrow.glb", game.debugLog, game.globalArena).meshes[0];
	gizmo->scaleArrowMesh = engine.CreateMesh(materialIndex, scaleArrowMeshFile.vertices, scaleArrowMeshFile.vertexCount, scaleArrowMeshFile.meshHash);

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
		//translateArrow->InitCollisionBody(game.physicsWorld);
		//translateArrow->InitBoxCollider(game.physicsCommon, { 0.1f, 1.f, 0.1f }, { 0.f, .5f, 0.f }, CollisionLayers::GizmoClick);
		translateArrow->SetLocalRotation(xyzRotations[i]);
		translateArrow->name = std::format("TranslateArrow{}", xyzNames[i]).c_str();
		translateArrow->isGizmoTranslationArrow = true;
		translateArrow->gizmoTranslationAxis = transformAxis[i];
		gizmo->root->AddChild(translateArrow, false);

		Entity* rotateArrow = gizmo->rotateArrows[i];
		rotateArrow = game.CreateMeshEntity(engine, materialIndex, gizmo->rotateArrowMesh);
		rotateArrow->GetBuffer().color = xyzColors[i];
		//rotateArrow->InitCollisionBody(game.physicsWorld);
		//rotateArrow->InitBoxCollider(game.physicsCommon, { 2.f, .05f, 2.f }, { 0.f, 0.f, 0.f }, CollisionLayers::GizmoClick);
		rotateArrow->SetLocalRotation(xyzRotations[i]);
		rotateArrow->name = std::format("RotateArrow{}", xyzNames[i]).c_str();
		rotateArrow->isGizmoRotationRing = true;
		rotateArrow->gizmoRotationAxis = transformAxis[i];
		gizmo->root->AddChild(rotateArrow, false);

		Entity* scaleArrow = gizmo->scaleArrows[i];
		scaleArrow = game.CreateMeshEntity(engine, materialIndex, gizmo->scaleArrowMesh);
		scaleArrow->GetBuffer().color = xyzColors[i];
		//scaleArrow->InitCollisionBody(game.physicsWorld);
		//scaleArrow->InitBoxCollider(game.physicsCommon, { .15f, .5f, .15f }, { .0f, .25f, .0f }, CollisionLayers::GizmoClick);
		scaleArrow->SetLocalRotation(xyzRotations[i]);
		scaleArrow->name = std::format("ScaleArrow{}", xyzNames[i]).c_str();
		scaleArrow->isGizmoScaleCube = true;
		scaleArrow->gizmoScaleAxis = transformAxis[i];
		gizmo->root->AddChild(scaleArrow, false);
	}

	return gizmo;
}