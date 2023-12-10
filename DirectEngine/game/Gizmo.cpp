#include "Gizmo.h"
#include "Game.h"

void Gizmo::Init(MemoryArena& arena, IEntityCreator* game, EngineCore& engine, MaterialData* material)
{
	assert(material != nullptr);

	translateArrowMesh = engine.CreateMesh(LoadGltfFromFile("models/translate-arrow.glb", arena)->meshes[0].mesh);
	rotateArrowMesh = engine.CreateMesh(LoadGltfFromFile("models/rotate-arrow.glb", arena)->meshes[0].mesh);
	scaleArrowMesh = engine.CreateMesh(LoadGltfFromFile("models/scale-arrow.glb", arena)->meshes[0].mesh);

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

	root = game->CreateEmptyEntity(engine);
	root->SetActive(false);
	root->name = "Gizmo";

	for (int i = 0; i < 3; i++)
	{
		Entity* translateArrow = translateArrows[i];
		translateArrow = game->CreateMeshEntity(engine, material, translateArrowMesh);
		translateArrow->GetBuffer().color = xyzColors[i];
		//translateArrow->InitCollisionBody(game->physicsWorld);
		//translateArrow->InitBoxCollider(game->physicsCommon, { 0.1f, 1.f, 0.1f }, { 0.f, .5f, 0.f }, CollisionLayers::GizmoClick);
		translateArrow->SetLocalRotation(xyzRotations[i]);
		translateArrow->name = std::format("TranslateArrow{}", xyzNames[i]).c_str();
		translateArrow->isGizmoTranslationArrow = true;
		translateArrow->gizmoTranslationAxis = transformAxis[i];
		root->AddChild(translateArrow, false);

		Entity* rotateArrow = rotateArrows[i];
		rotateArrow = game->CreateMeshEntity(engine, material, rotateArrowMesh);
		rotateArrow->GetBuffer().color = xyzColors[i];
		//rotateArrow->InitCollisionBody(game->physicsWorld);
		//rotateArrow->InitBoxCollider(game->physicsCommon, { 2.f, .05f, 2.f }, { 0.f, 0.f, 0.f }, CollisionLayers::GizmoClick);
		rotateArrow->SetLocalRotation(xyzRotations[i]);
		rotateArrow->name = std::format("RotateArrow{}", xyzNames[i]).c_str();
		rotateArrow->isGizmoRotationRing = true;
		rotateArrow->gizmoRotationAxis = transformAxis[i];
		root->AddChild(rotateArrow, false);

		Entity* scaleArrow = scaleArrows[i];
		scaleArrow = game->CreateMeshEntity(engine, material, scaleArrowMesh);
		scaleArrow->GetBuffer().color = xyzColors[i];
		//scaleArrow->InitCollisionBody(game->physicsWorld);
		//scaleArrow->InitBoxCollider(game->physicsCommon, { .15f, .5f, .15f }, { .0f, .25f, .0f }, CollisionLayers::GizmoClick);
		scaleArrow->SetLocalRotation(xyzRotations[i]);
		scaleArrow->name = std::format("ScaleArrow{}", xyzNames[i]).c_str();
		scaleArrow->isGizmoScaleCube = true;
		scaleArrow->gizmoScaleAxis = transformAxis[i];
		root->AddChild(scaleArrow, false);
	}
}

void Gizmo::Update(EngineInput& input)
{
	/*if (editMode && editElement != nullptr && input.KeyJustPressed(VK_LBUTTON))
	{
		RaycastScreenPosition(engine, *engine.mainCamera, {input.mouseX, input.mouseY}, &allRaycastCollector, CollisionLayers::GizmoClick);
		Entity* selectedGizmo = nullptr;
		for (CollisionRecord& gizmoHit : allRaycastCollector.collisions)
		{
			if (gizmoHit.collider->getUserData() != nullptr)
			{
				Entity* hitGizmo = reinterpret_cast<Entity*>(gizmoHit.collider->getUserData());
				if (selectedGizmo == nullptr
					|| (selectedGizmo->isGizmoRotationRing && (hitGizmo->isGizmoTranslationArrow || hitGizmo->isGizmoScaleCube))
					|| (selectedGizmo->isGizmoTranslationArrow && hitGizmo->isGizmoScaleCube))
				{
					selectedGizmo = hitGizmo;
				}
			}
		}

		if (selectedGizmo != nullptr)
		{
			gizmoDragCursorStart = { input.mouseX, input.mouseY };
			selectedGizmoElement = selectedGizmo;
			selectedGizmoTarget = editElement;

			if (selectedGizmo->isGizmoTranslationArrow)
			{
				gizmoDragEntityStart = selectedGizmoTarget->GetLocalPosition();
			}
			else if (selectedGizmo->isGizmoRotationRing)
			{
				gizmoDragEntityStart = selectedGizmoTarget->GetLocalRotation();
			}
			else if (selectedGizmo->isGizmoScaleCube)
			{
				gizmoDragEntityStart = selectedGizmoTarget->GetLocalScale();
			}
		}
	}
	else if (selectedGizmoElement != nullptr && selectedGizmoTarget != nullptr)
	{
		XMVECTOR dragOffsetScreen = XMVECTOR{ input.mouseX, input.mouseY } - gizmoDragCursorStart;
		XMVECTOR cursorStart = ScreenToWorldPosition(engine, *engine.mainCamera, gizmoDragCursorStart);
		XMVECTOR cursorEnd = ScreenToWorldPosition(engine, *engine.mainCamera, gizmoDragCursorStart + dragOffsetScreen);
		XMVECTOR cursorMovement = (cursorEnd - cursorStart) * 10.f;

		if (selectedGizmoElement->isGizmoTranslationArrow)
		{
			XMVECTOR projectedMovement = XMVector3Dot(cursorMovement, selectedGizmoElement->gizmoTranslationAxis) * selectedGizmoElement->gizmoTranslationAxis;
			selectedGizmoTarget->SetLocalPosition(gizmoDragEntityStart + projectedMovement);
		}
		if (selectedGizmoElement->isGizmoRotationRing)
		{
			selectedGizmoTarget->SetLocalRotation(XMQuaternionMultiply(gizmoDragEntityStart, XMQuaternionRotationAxis(selectedGizmoElement->gizmoRotationAxis, XMVectorGetX(dragOffsetScreen) * .01f)));
		}
		if (selectedGizmoElement->isGizmoScaleCube)
		{
			XMVECTOR projectedMovement = XMVector3Dot(cursorMovement, selectedGizmoElement->gizmoScaleAxis) * selectedGizmoElement->gizmoScaleAxis;
			selectedGizmoTarget->SetLocalScale(gizmoDragEntityStart + projectedMovement);
		}

		if (!editMode)
		{
			selectedGizmoElement = nullptr;
			selectedGizmoTarget = nullptr;
		}
	}*/

	if (input.KeyJustReleased(VK_LBUTTON))
	{
		selectedGizmoElement = nullptr;
		selectedGizmoTarget = nullptr;
	}
}
