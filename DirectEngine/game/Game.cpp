#include "Game.h"

#include "../Helpers.h"

#include <array>
#include <format>

#include "../core/vkcodes.h"
#include "remixicon.h"

void Game::StartGame(EngineCore& engine)
{
	INIT_TIMER(timer);

	ImGui::SetCurrentContext(engine.m_imguiContext);
	LoadUIStyle();

	// Physics
	physicsWorld = physicsCommon.createPhysicsWorld();
	physicsWorld->setIsDebugRenderingEnabled(true);
	reactphysics3d::DebugRenderer& debugRenderer = physicsWorld->getDebugRenderer();
	debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLIDER_AABB, true);
	debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
	debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::CONTACT_POINT, true);
	debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::CONTACT_NORMAL, true);


	// Shaders
	std::wstring defaultShader = L"entity";
	std::wstring groundShader = L"ground";
	std::wstring laserShader = L"laser";
	std::wstring crosshairShader = L"crosshair";
	std::wstring textureQuad = L"texturequad";

	// Textures
	diffuseTexture = engine.CreateTexture(L"textures/ground_D.dds");
	memeTexture = engine.CreateTexture(L"textures/cat_D.dds");

	LOG_TIMER(timer, "Test Textures", debugLog);
	RESET_TIMER(timer);

	// Materials
	size_t memeMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { memeTexture }, defaultShader);
	size_t groundMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, groundShader);
	size_t laserMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, laserShader);
	size_t portal1MaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &engine.m_renderTextures[0]->texture}, textureQuad);
	size_t portal2MaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &engine.m_renderTextures[1]->texture}, textureQuad);
	size_t crosshairMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, crosshairShader);

	LOG_TIMER(timer, "Test Materials", debugLog);
	RESET_TIMER(timer);

	// Meshes
	// TODO: why does mesh need material index, and why doesn't it matter if it's wrong?
	MeshFile cubeMeshFile = LoadGltfFromFile("models/cube.glb", debugLog, modelArena).meshes[0];
	auto cubeMeshView = engine.CreateMesh(memeMaterialIndex, cubeMeshFile.vertices, cubeMeshFile.vertexCount);
	engine.cubeVertexView = cubeMeshView;

	gizmo = LoadGizmo(engine, *this, laserMaterialIndex);
	
	LOG_TIMER(timer, "Default Meshes", debugLog);
	RESET_TIMER(timer);

	// Entities
	Entity* crosshair = CreateQuadEntity(engine, crosshairMaterialIndex, .03f, .03f, true);
	crosshair->GetData().mainOnly = true;
	crosshair->GetBuffer().color = { 1.f, .3f, .1f, 1.f };

	auto createPortal = [&](size_t matIndex){
		Entity* portalRenderQuad = CreateQuadEntity(engine, matIndex, 2.f, 4.f);
		portalRenderQuad->position = { -1.f, 2.f, 0.f };
		portalRenderQuad->rotation = XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0.f, 0.f);
		portalRenderQuad->name = "PortalQuad";

		Entity* portal = CreateEmptyEntity(engine);
		portal->rotation = XMQuaternionRotationRollPitchYaw(0.f, XM_PI, 0.f);
		portal->AddChild(portalRenderQuad);

		return portal;
	};
	portal1 = createPortal(portal2MaterialIndex);
	portal1->position = { 2.f, 2.f, 3.f };
	portal1->name = "Portal 1";

	portal2 = createPortal(portal1MaterialIndex);
	portal2->position = { -2.f, 2.f, 3.f };
	portal2->name = "Portal 2";

	playerEntity = CreateEmptyEntity(engine);
	playerEntity->name = "Player";
	playerEntity->position = { 0.f, 1.f, 0.f };
	playerEntity->InitRigidBody(physicsWorld, reactphysics3d::BodyType::DYNAMIC);
	playerEntity->InitBoxCollider(physicsCommon, { .5f, 1.f, .5f }, { 0.f, 1.f, 0.f }, CollisionLayers::Player);
	playerEntity->rigidBody->setAngularLockAxisFactor({ 0.f, 0.f, 0.f });
	playerEntity->rigidBody->enableGravity(false);

	playerLookEntity = CreateEmptyEntity(engine);
	playerLookEntity->name = "PlayerLook";
	playerLookEntity->position = { 0.f, 1.85f, 0.f };
	playerEntity->AddChild(playerLookEntity);

	cameraEntity = CreateEmptyEntity(engine);
	cameraEntity->name = "Camera";
	cameraEntity->position = { 0.f, 0.f, 0.15f };
	playerLookEntity->AddChild(cameraEntity);

	LOG_TIMER(timer, "Camera Entity", debugLog);
	RESET_TIMER(timer);

	playerModelEntity = CreateEntityFromGltf(engine, "models/kaiju.glb", defaultShader, debugLog);

	LOG_TIMER(timer, "Kaiju Entity", debugLog);
	RESET_TIMER(timer);

	assert(playerModelEntity->isSkinnedRoot);
	playerModelEntity->name = "KaijuRoot";
	playerModelEntity->transformHierachy->SetAnimationActive("BasePose", true);
	playerModelEntity->transformHierachy->SetAnimationActive("NeckShrink", true);

	playerEntity->AddChild(playerModelEntity);

	for (int i = 0; i < playerModelEntity->childCount; i++)
	{
		Entity* entity = playerModelEntity->children[i];
	}

	LOG_TIMER(timer, "Kaiju Children", debugLog);
	RESET_TIMER(timer);

	Entity* groundEntity = CreateQuadEntity(engine, groundMaterialIndex, 100.f, 100.f, false, CollisionInitType::RigidBody, CollisionLayers::Floor);
	groundEntity->name = "Ground";
	groundEntity->GetBuffer().color = { 1.f, 1.f, 1.f };
	groundEntity->position = XMVectorSetY(groundEntity->position, -.01f);
	groundEntity->rigidBody->setType(reactphysics3d::BodyType::STATIC);

	for (int i = 0; i < 16; i++)
	{
		Entity* yea = CreateMeshEntity(engine, groundMaterialIndex, cubeMeshView);
		yea->name = "Yea";
		yea->position = { 3.f, 0.5f + i * 16.f, 0.f };
		yea->InitRigidBody(physicsWorld);
		yea->InitBoxCollider(physicsCommon, { .5f, .5f, .5f }, {}, CollisionLayers::Floor);
	}

	// Defaults
	playerPitch = XM_PI;

	light.position = { 10.f, 10.f, 10.f };

	LOG_TIMER(timer, "Other Entities", debugLog);
	RESET_TIMER(timer);

	// Audio
	soundFiles[(size_t)AudioFile::PlayerDamage] = LoadAudioFile(L"audio/chord.wav", globalArena);
	soundFiles[(size_t)AudioFile::EnemyDeath] = LoadAudioFile(L"audio/tada.wav", globalArena);
	soundFiles[(size_t)AudioFile::Shoot] = LoadAudioFile(L"audio/laser.wav", globalArena);
	
	// Finish setup
	engine.UploadVertices();
	UpdateCursorState();

	LOG_TIMER(timer, "Finalize", debugLog);
	RESET_TIMER(timer);
}

void Game::UpdateGame(EngineCore& engine)
{
	engine.BeginProfile("Game Update", ImColor::HSV(.75f, 1.f, 1.f));
	input.accessMutex.lock();
	input.UpdateMousePosition();

	// Read camera rotation into vectors
	XMVECTOR camForward, camRight, camUp;
	CalculateDirectionVectors(camForward, camRight, camUp, engine.mainCamera->rotation);

	// Reset per frame values
	engine.m_debugLineData.lineVertices.Clear();

	// Debug Controls
	if (input.KeyJustPressed(VK_ESCAPE))
	{
		showEscMenu = !showEscMenu;
		UpdateCursorState();
	}
	if (input.KeyComboJustPressed(VK_KEY_D, VK_CONTROL))
	{
		showDebugUI = !showDebugUI;
	}
	if (input.KeyComboJustPressed(VK_KEY_L, VK_CONTROL))
	{
		showLog = !showLog;
	}
	if (input.KeyComboJustPressed(VK_KEY_P, VK_CONTROL))
	{
		showProfiler = !showProfiler;
	}
	if (input.KeyComboJustPressed(VK_KEY_T, VK_CONTROL))
	{
		showDebugImage = !showDebugImage;
	}

	// Camera controls
	if (!showEscMenu)
	{
		constexpr float maxPitch = XMConvertToRadians(80.f);

		playerYaw += input.mouseDeltaX * 0.003f;
		playerPitch += input.mouseDeltaY * 0.003f;
		if (playerPitch > maxPitch) playerPitch = maxPitch;
		if (playerPitch < -maxPitch) playerPitch = -maxPitch;
	}

	cameraEntity->rotation = XMQuaternionRotationRollPitchYaw(playerPitch, 0.f, 0.f);
	playerLookEntity->rotation = XMQuaternionRotationRollPitchYaw(0.f, playerYaw, 0.f);

	XMVECTOR horizontalCamForward = XMVectorSetY(camForward, 0.f);
	if (XMVectorGetX(XMVector3AngleBetweenVectors(horizontalCamForward, playerModelEntity->GetForwardDirection())) > XMConvertToRadians(20.f))
	{
		playerModelEntity->SetForwardDirection(XMVector3Normalize(horizontalCamForward));
	}

	// Player movement
	if (input.KeyComboJustPressed(VK_KEY_R, VK_CONTROL))
	{
		playerEntity->position = { 0.f, 1.f, 0.f };
		playerEntity->rigidBody->setTransform(PhysicsTransformFromXM(playerEntity->position, playerEntity->rotation));
		playerEntity->rigidBody->setLinearVelocity({ 0.f, 0.f, 0.f });
		playerEntity->rigidBody->setAngularVelocity({ 0.f, 0.f, 0.f });
	}

	float horizontalInput = 0.f;
	if (input.KeyDown(VK_KEY_A)) horizontalInput -= 1.f;
	if (input.KeyDown(VK_KEY_D)) horizontalInput += 1.f;

	float verticalInput = 0.f;
	if (input.KeyDown(VK_KEY_S)) verticalInput -= 1.f;
	if (input.KeyDown(VK_KEY_W)) verticalInput += 1.f;

	playerEntity->rigidBody->setIsActive(!noclip);

	if (noclip)
	{
		float camSpeed = 10.f;
		if (input.KeyDown(VK_SHIFT)) camSpeed *= .1f;
		if (input.KeyDown(VK_CONTROL)) camSpeed *= 5.f;

		playerEntity->position += camRight * horizontalInput * engine.m_updateDeltaTime * camSpeed;
		playerEntity->position += camForward * verticalInput * engine.m_updateDeltaTime * camSpeed;
	}
	else
	{
		// Buffer jump for later
		if (input.KeyJustPressed(VK_SPACE))
		{
			lastJumpPressTime = engine.TimeSinceStart();
		}

		// Add input to velocity
		XMVECTOR playerForward = XMVector3Normalize(XMVectorSetY(camForward, 0.f));
		XMVECTOR playerRight = XMVector3Normalize(XMVectorSetY(camRight, 0.f));

		XMVECTOR wantedForward = XMVectorSetY(XMVectorScale(playerForward, verticalInput), 0.f);
		XMVECTOR wantedSideways = XMVectorSetY(XMVectorScale(playerRight, horizontalInput), 0.f);
		XMVECTOR wantedDirection = XMVector3Normalize(wantedForward + wantedSideways);
		bool playerWantsDirection = XMVectorGetX(XMVector3LengthSq(wantedForward) + XMVector3LengthSq(wantedSideways)) > 0.01f;

		XMVECTOR playerVelocity = XMVectorFromPhysics(playerEntity->rigidBody->getLinearVelocity());
		float verticalPlayerVelocity = XMVectorGetY(playerVelocity);
		XMVECTOR horizontalPlayerVelocity = XMVectorSetY(playerVelocity, 0.f);
		XMVECTOR horizontalPlayerDirection = XMVector3Normalize(XMVectorSetY(playerVelocity, 0.f));
		float horizontalPlayerSpeed = XMVectorGetX(XMVector3Length(horizontalPlayerVelocity));

		// Acceleration
		if (playerWantsDirection)
		{
			XMVECTOR resultDirection = wantedDirection;
			float resultScale = 1.;
			if (horizontalPlayerSpeed >= 0.01f)
			{
				resultScale = std::max(0.f, XMVectorGetX(XMVector3Dot(XMVector3Normalize(horizontalPlayerVelocity), wantedDirection)));
			}

			horizontalPlayerSpeed = horizontalPlayerSpeed * resultScale + playerAcceleration * engine.m_updateDeltaTime;
			if (horizontalPlayerSpeed > playerMaxSpeed)
			{
				horizontalPlayerSpeed = playerMaxSpeed;
			}

			playerVelocity = XMVectorScale(resultDirection, horizontalPlayerSpeed);
			playerVelocity = XMVectorSetY(playerVelocity, verticalPlayerVelocity);
		}

		// Ground collision
		const float collisionEpsilon = .02f;
		minRaycastCollector.Raycast(physicsWorld, playerEntity->position + V3_UP * collisionEpsilon, playerEntity->position - V3_UP * collisionEpsilon, CollisionLayers::Floor);
		playerOnGround = minRaycastCollector.anyCollision;

		if (playerOnGround)
		{
			// Move player out of ground
			playerEntity->position = playerEntity->position + XMVectorScale(V3_UP, collisionEpsilon - minRaycastCollector.collision.distance);

			// Stop falling speed
			playerVelocity = XMVectorSetY(playerVelocity, 0.);

			// Apply jump
			if (input.KeyDown(VK_SPACE) && (lastJumpPressTime + jumpBufferDuration >= engine.TimeSinceStart() || autojump))
			{
				lastJumpPressTime = -1000.f;
				playerVelocity.m128_f32[1] += playerJumpStrength;
			}
			else
			{
				// Apply friction when no input
				if (std::abs(horizontalInput) <= inputDeadzone && std::abs(verticalInput) <= inputDeadzone)
				{
					float speedDecrease = playerFriction * engine.m_updateDeltaTime;
					if (horizontalPlayerSpeed <= speedDecrease)
					{
						playerVelocity = V3_ZERO;
					}
					playerVelocity = XMVectorScale(XMVector3Normalize(playerVelocity), horizontalPlayerSpeed - speedDecrease);
				}
			}
		}
		else
		{
			playerVelocity.m128_f32[1] -= engine.m_updateDeltaTime * playerGravity;
		}

		// Apply velocity
		playerEntity->rigidBody->setLinearVelocity(PhysicsVectorFromXM(playerVelocity));
	}

	// Gizmo
	if (editMode && editElement != nullptr && input.KeyJustPressed(VK_LBUTTON))
	{
		RaycastScreenPosition(engine, *engine.mainCamera, { input.mouseX, input.mouseY }, &allRaycastCollector, CollisionLayers::GizmoClick);
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
				gizmoDragEntityStart = selectedGizmoTarget->position;
			}
			else if (selectedGizmo->isGizmoRotationRing)
			{
				gizmoDragEntityStart = selectedGizmoTarget->rotation;
			}
			else if (selectedGizmo->isGizmoScaleCube)
			{
				gizmoDragEntityStart = selectedGizmoTarget->scale;
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
			selectedGizmoTarget->position = gizmoDragEntityStart + projectedMovement;
		}
		if (selectedGizmoElement->isGizmoRotationRing)
		{
			selectedGizmoTarget->rotation = XMQuaternionMultiply(gizmoDragEntityStart, XMQuaternionRotationAxis(selectedGizmoElement->gizmoRotationAxis, XMVectorGetX(dragOffsetScreen) * .01f));
		}
		if (selectedGizmoElement->isGizmoScaleCube)
		{
			XMVECTOR projectedMovement = XMVector3Dot(cursorMovement, selectedGizmoElement->gizmoScaleAxis) * selectedGizmoElement->gizmoScaleAxis;
			selectedGizmoTarget->scale = gizmoDragEntityStart + projectedMovement;
		}

		if (!editMode)
		{
			selectedGizmoElement = nullptr;
			selectedGizmoTarget = nullptr;
		}
	}
	
	if (input.KeyJustReleased(VK_LBUTTON))
	{
		selectedGizmoElement = nullptr;
		selectedGizmoTarget = nullptr;
	}

	// Player Audio
	XMStoreFloat3(&playerAudioListener.OrientFront, camForward);
	XMStoreFloat3(&playerAudioListener.OrientTop, XMVector3Cross(camForward, camRight));
	XMStoreFloat3(&playerAudioListener.Position, engine.mainCamera->position);
	XMStoreFloat3(&playerAudioListener.Velocity, XMVectorFromPhysics(playerEntity->rigidBody->getLinearVelocity()));

	// Shoot portal
	if (input.KeyJustPressed(VK_LBUTTON) || input.KeyJustPressed(VK_RBUTTON))
	{
		minRaycastCollector.Raycast(physicsWorld, engine.mainCamera->position, engine.mainCamera->position + camForward * 1000.f, CollisionLayers::Floor);
		if (minRaycastCollector.anyCollision)
		{
			Entity* targetPortal = input.KeyJustPressed(VK_LBUTTON) ? portal1 : portal2;
			targetPortal->position = minRaycastCollector.collision.worldPoint - camForward * 0.001f;
			targetPortal->SetForwardDirection(minRaycastCollector.collision.worldNormal);
		}
	}

	// Update entities
	for (Entity& entity : entityArena)
	{
		entity.UpdateAnimation(engine, false);
	}

	// Update physics
	for (Entity& entity : entityArena)
	{
		if (entity.collisionBody != nullptr)
		{
			entity.collisionBody->setTransform(PhysicsTransformFromXM(entity.position, entity.rotation));
		}
		if (entity.rigidBody != nullptr && entity.rigidBody->getType() == reactphysics3d::BodyType::STATIC)
		{
			entity.rigidBody->setTransform(PhysicsTransformFromXM(entity.position, entity.rotation));
		}
	}

	physicsWorld->update(std::min(engine.m_updateDeltaTime, MAX_PHYSICS_STEP));

	if (renderPhysics)
	{
		reactphysics3d::DebugRenderer& debugRenderer = physicsWorld->getDebugRenderer();
		for (int i = 0; i < debugRenderer.getNbLines(); i++)
		{
			auto line = debugRenderer.getLinesArray()[i];
			engine.m_debugLineData.AddLine(XMVectorFromPhysics(line.point1), XMVectorFromPhysics(line.point2), XMColorFromPhysics(line.color1), XMColorFromPhysics(line.color2));
		}
	}

	for (Entity& entity : entityArena)
	{
		if (entity.rigidBody == nullptr) continue;
		const reactphysics3d::Transform& rbTransform = entity.rigidBody->getTransform();
		const reactphysics3d::Vector3& rbPosition = rbTransform.getPosition();
		const reactphysics3d::Quaternion& rbOrientation = rbTransform.getOrientation();
		entity.position = { rbPosition.x, rbPosition.y, rbPosition.z };
		entity.rotation = { rbOrientation.x, rbOrientation.y, rbOrientation.z, rbOrientation.w };
	}

	// Update entity matrices
	for (Entity& entity : entityArena)
	{
		if (entity.parent == nullptr)
		{
			entity.UpdateWorldMatrix();
			entity.UpdateAudio(engine, &playerAudioListener);
		}
	}

	// Update Camera
	XMVECTOR camTranslation, camRotation, camScale;
	XMMatrixDecompose(&camScale, &camRotation, &camTranslation, cameraEntity->worldMatrix);
	engine.mainCamera->position = camTranslation;
	engine.mainCamera->rotation = camRotation;

	engine.m_renderTextures[0]->camera->position = portal1->position;
	engine.m_renderTextures[0]->camera->rotation = portal1->rotation;
	engine.m_renderTextures[1]->camera->position = portal2->position;
	engine.m_renderTextures[1]->camera->rotation = portal2->rotation;

	CalculateDirectionVectors(camForward, camRight, camUp, engine.mainCamera->rotation);

	for (uint32_t i = 0; i < engine.m_cameraCount; i++)
	{
		CameraConstantBuffer& cambuf = engine.m_cameras[i].constantBuffer.data;
		cambuf.postProcessing = { contrast, brightness, saturation, fog };
		cambuf.fogColor = clearColor;
	}

	// Update light/shadowmap
	light.rotation = XMQuaternionRotationRollPitchYaw(45.f / 360.f * XM_2PI, engine.TimeSinceStart(), 0.f);

	MAT_RMAJ lightView = XMMatrixMultiply(XMMatrixTranslationFromVector(XMVectorScale(light.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(light.rotation)));
	engine.m_lightConstantBuffer.data.lightView = XMMatrixTranspose(lightView);

	if (showLightPosition)
	{
		engine.m_debugLineData.AddLine(light.position, light.position + XMVector3TransformNormal({ 0.f, 0.f, 1.f }, XMMatrixRotationQuaternion(light.rotation)), { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });
	}

	XMVECTOR lightRight, lightUp;
	CalculateDirectionVectors(engine.m_lightConstantBuffer.data.sunDirection, lightRight, lightUp, light.rotation);

	MAT_RMAJ camView = XMMatrixTranspose(engine.mainCamera->constantBuffer.data.cameraView);
	MAT_RMAJ camProj = XMMatrixTranspose(engine.mainCamera->constantBuffer.data.cameraProjection);
	MAT_RMAJ shadowSpaceMatrix = CalculateShadowCamProjection(camView, camProj, lightView);
	
	engine.m_lightConstantBuffer.data.lightProjection = XMMatrixTranspose(shadowSpaceMatrix);
	
	// Debug view for light space
	if (showLightSpaceDebug)
	{
		const XMVECTOR cubeVertices[8] = {
		{ NDC_MIN_XY, NDC_MIN_XY, NDC_MIN_Z },
		{ NDC_MIN_XY, NDC_MIN_XY, NDC_MAX_Z },
		{ NDC_MIN_XY, NDC_MAX_XY, NDC_MIN_Z },
		{ NDC_MIN_XY, NDC_MAX_XY, NDC_MAX_Z },
		{ NDC_MAX_XY, NDC_MIN_XY, NDC_MIN_Z },
		{ NDC_MAX_XY, NDC_MIN_XY, NDC_MAX_Z },
		{ NDC_MAX_XY, NDC_MAX_XY, NDC_MIN_Z },
		{ NDC_MAX_XY, NDC_MAX_XY, NDC_MAX_Z },
		};

		const XMINT2 cubeEdges[12] = {
			{ 0, 1 },
			{ 0, 2 },
			{ 0, 4 },
			{ 1, 3 },
			{ 1, 5 },
			{ 2, 3 },
			{ 2, 6 },
			{ 3, 7 },
			{ 4, 5 },
			{ 4, 6 },
			{ 5, 7 },
			{ 6, 7 },
		};

		XMVECTOR det;
		XMMATRIX lightViewInverse = XMMatrixInverse(&det, XMMatrixTranspose(engine.m_lightConstantBuffer.data.lightView));
		XMMATRIX lightProjectionInverse = XMMatrixInverse(&det, XMMatrixTranspose(engine.m_lightConstantBuffer.data.lightProjection));
		XMFLOAT4 cubeColor = { 1., 1., 1., 1. };

		for (int i = 0; i < _countof(cubeEdges); i++)
		{
			XMVECTOR edgeStartView = XMVector3Transform(cubeVertices[cubeEdges[i].x], lightProjectionInverse);
			XMVECTOR edgeEndView = XMVector3Transform(cubeVertices[cubeEdges[i].y], lightProjectionInverse);
			XMVECTOR edgeStart = XMVector3Transform(edgeStartView, lightViewInverse);
			XMVECTOR edgeEnd = XMVector3Transform(edgeEndView, lightViewInverse);

			XMFLOAT3 edgeStart3;
			XMFLOAT3 edgeEnd3;
			XMStoreFloat3(&edgeStart3, edgeStart);
			XMStoreFloat3(&edgeEnd3, edgeEnd);
			engine.m_debugLineData.lineVertices.NewElement() = { edgeStart3, cubeColor };
			engine.m_debugLineData.lineVertices.NewElement() = { edgeEnd3, cubeColor };
		}
	}

	// Post Processing
	XMVECTOR camPos2d = engine.mainCamera->position;
	camPos2d.m128_f32[1] = 0.f;
	float distanceFromSpawn = XMVector3Length(camPos2d).m128_f32[0];
	float maxDistance = 40.f;
	fog = distanceFromSpawn * 3.f / maxDistance;
	float colorLerp = std::min(1.f, distanceFromSpawn / maxDistance);
	clearColor = XMVectorLerp(baseClearColor, fogColor, colorLerp);

	engine.EndProfile("Game Update");
	// Draw UI
	if (!pauseProfiler)
	{
		lastDebugFrameIndex = (lastDebugFrameIndex + 1) % _countof(lastFrames);
		lastFrames[lastDebugFrameIndex].duration = static_cast<float>(engine.m_updateDeltaTime);
	}

	DrawUI(engine);

	input.NextFrame();
	input.accessMutex.unlock();
}

void Game::BeforeMainRender(EngineCore& engine)
{
	for (Entity& entity : entityArena)
	{
		entity.UpdateAnimation(engine, true);
	}
}

float* Game::GetClearColor()
{
	return clearColor.m128_f32;
}

EngineInput& Game::GetInput()
{
	return input;
}

Entity* Game::CreateEmptyEntity(EngineCore& engine)
{
	Entity* entity = NewObject(entityArena, Entity);
	entity->engine = &engine;
	entity->isRendered = false;
	return entity;
}

Entity* Game::CreateMeshEntity(EngineCore& engine, size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshView)
{
	Entity* entity = NewObject(entityArena, Entity);
	entity->engine = &engine;
	entity->isRendered = true;
	entity->materialIndex = materialIndex;
	entity->dataIndex = engine.CreateEntity(materialIndex, meshView);
	return entity;
}

Entity* Game::CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, bool vertical, CollisionInitType collisionInit, CollisionLayers collisionLayers)
{
	MeshFile file = vertical ? CreateQuadY(width, height, modelArena) : CreateQuad(width, height, modelArena);
	auto meshView = engine.CreateMesh(materialIndex, file.vertices, file.vertexCount);
	Entity* entity = CreateMeshEntity(engine, materialIndex, meshView);
	entity->position = { -width / 2.f, 0.f, -height / 2.f };
	if (collisionInit == CollisionInitType::CollisionBody)
	{
		entity->InitCollisionBody(physicsWorld);
	}
	if (collisionInit == CollisionInitType::RigidBody)
	{
		entity->InitRigidBody(physicsWorld);
	}
	if (collisionInit != CollisionInitType::None)
	{
		reactphysics3d::Collider* collider = entity->InitBoxCollider(physicsCommon, { width / 2.f, .2f, height / 2.f }, { width / 2.f, -.2f, height / 2.f }, collisionLayers);
	}
	return entity;
}

Entity* Game::CreateEntityFromGltf(EngineCore& engine, const char* path, const std::wstring& shaderName, RingLog& log)
{
	Entity* mainEntity = CreateEmptyEntity(engine);

	GltfResult gltfResult = LoadGltfFromFile(path, log, modelArena);
	if (gltfResult.transformHierachy != nullptr)
	{
		mainEntity->isSkinnedRoot = true;
		mainEntity->transformHierachy = gltfResult.transformHierachy;
	}

	INIT_TIMER(timer);

	for (MeshFile& meshFile : gltfResult.meshes)
	{
		// TODO: read shader root signaure instead, check number/type of texture, load textures accordingly
		std::vector<Texture*> textures{};
		for (int i = 0; i < meshFile.textureCount; i++)
		{
			std::wstring texturePath = { L"textures/" };
			texturePath.append(meshFile.textureName.begin(), meshFile.textureName.end());
			texturePath.append(L".dds");

			textures.push_back(engine.CreateTexture(texturePath.c_str()));
		}
		LOG_TIMER(timer, "Texture for model", debugLog);
		RESET_TIMER(timer);

		size_t materialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), textures, shaderName);
		D3D12_VERTEX_BUFFER_VIEW meshView = engine.CreateMesh(materialIndex, meshFile.vertices, meshFile.vertexCount);
		LOG_TIMER(timer, "Create material and mesh for model", debugLog);
		RESET_TIMER(timer);

		Entity* child = CreateMeshEntity(engine, materialIndex, meshView);
		child->name = meshFile.textureName.c_str();
		if (gltfResult.transformHierachy != nullptr) child->isSkinnedMesh = true;
		mainEntity->AddChild(child);

		LOG_TIMER(timer, "Create entity for model", debugLog);
		RESET_TIMER(timer);
	}

	return mainEntity;
}

void Game::UpdateCursorState()
{
	windowUdpateDataMutex.lock();
	windowUpdateData.updateCursor = true;
	windowUpdateData.cursorClipped = !showEscMenu;
	windowUpdateData.cursorVisible = showEscMenu;
	windowUdpateDataMutex.unlock();

	ImGuiConfigFlags& flags = ImGui::GetIO().ConfigFlags;
	if (showEscMenu)
	{
		flags &= ~(ImGuiConfigFlags_NoMouse);
	}
	else
	{
		flags |= ImGuiConfigFlags_NoMouse;
	}
}

void Game::PlaySound(EngineCore& engine, AudioSource* audioSource, AudioFile file)
{
	audioSource->PlaySound(engine.m_audio, soundFiles[(size_t)file]);
}

XMVECTOR Game::ScreenToWorldPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos)
{
	XMMATRIX cameraView = XMMatrixTranspose(cameraData.constantBuffer.data.cameraView);
	XMMATRIX cameraProjection = XMMatrixTranspose(cameraData.constantBuffer.data.cameraProjection);
	return XMVector3Unproject(screenPos, 0.f, 0.f, engine.m_width, engine.m_height, 0.f, 1.f, cameraProjection, cameraView, XMMatrixIdentity());
}

void Game::RaycastScreenPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos, EngineRaycastCallback* callback, CollisionLayers layers)
{
	XMVECTOR rayOriginWorld = ScreenToWorldPosition(engine, cameraData, screenPos);
	XMVECTOR rayDirection = XMVector3Normalize(rayOriginWorld - cameraData.position);
	callback->Raycast(physicsWorld, rayOriginWorld, rayOriginWorld + rayDirection * 1000.f);
}

void Game::Log(const std::string& message)
{
	if (!stopLog) debugLog.Log(message);
}

void Game::Warn(const std::string& message)
{
	if (!stopLog) debugLog.Warn(message);
}

void Game::Error(const std::string& message)
{
	if (!stopLog) debugLog.Error(message);
}

MAT_RMAJ CalculateShadowCamProjection(const MAT_RMAJ& camViewMatrix, const MAT_RMAJ& camProjectionMatrix, const MAT_RMAJ& lightViewMatrix)
{
	XMVECTOR ndcTestPoints[] = {
		{ -1.f, -1.f, 0.f, 1.f },
		{  1.f, -1.f, 0.f, 1.f },
		{ -1.f,  1.f, 0.f, 1.f },
		{  1.f,  1.f, 0.f, 1.f },
		{ -1.f, -1.f, 1.f, 1.f },
		{  1.f, -1.f, 1.f, 1.f },
		{ -1.f,  1.f, 1.f, 1.f },
		{  1.f,  1.f, 1.f, 1.f },
	};

	XMVECTOR worldTestPoints[8];
	for (size_t i = 0; i < 8; i++)
	{
		XMVECTOR camViewPoint = XMVector3TransformCoord(ndcTestPoints[i], XMMatrixInverse(nullptr, camProjectionMatrix));
		worldTestPoints[i] = XMVector3TransformCoord(camViewPoint, XMMatrixInverse(nullptr, camViewMatrix));
	}

	XMVECTOR lightSpaceTestPoints[8];
	for (size_t i = 0; i < 8; i++)
	{
		lightSpaceTestPoints[i] = XMVector3Transform(worldTestPoints[i], lightViewMatrix);
	}

	XMVECTOR lsMin = lightSpaceTestPoints[0];
	XMVECTOR lsMax = lightSpaceTestPoints[0];

	for (size_t i = 1; i < 8; i++)
	{
		lsMin = XMVectorMin(lsMin, lightSpaceTestPoints[i]);
		lsMax = XMVectorMax(lsMax, lightSpaceTestPoints[i]);
	}

	return XMMatrixOrthographicOffCenterLH(XMVectorGetX(lsMin), XMVectorGetX(lsMax), XMVectorGetY(lsMin), XMVectorGetY(lsMax), XMVectorGetZ(lsMin), XMVectorGetZ(lsMax));
}

IGame* CreateGame(MemoryArena& arena)
{
	return NewObject(arena, Game);
}