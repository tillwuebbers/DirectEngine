#include "Game.h"

#include "../Helpers.h"

#include <array>
#include <format>

#include "../core/vkcodes.h"
#include "remixicon.h"

Game::Game(GAME_CREATION_PARAMS) : globalArena(globalArena), configArena(configArena), levelArena(levelArena) {}

void Game::StartGame(EngineCore& engine)
{
	INIT_TIMER(timer);

	ImGui::SetCurrentContext(engine.m_imgui.imGuiContext);
	LoadUIStyle();

	// Config
	if (!LoadGameConfig())
	{
		ResetGameConfig();
		SaveConfig(configArena, CONFIG_PATH);
	}

	// Shaders
	std::wstring defaultShader = L"entity";
	std::wstring groundShader = L"ground";
	std::wstring laserShader = L"laser";
	std::wstring crosshairShader = L"crosshair";
	std::wstring textureQuad = L"texturequad";
	std::wstring portalShader = L"portal";

	// Textures
	diffuseTexture = engine.CreateTexture(L"textures/ground_D.dds");
	memeTexture = engine.CreateTexture(L"textures/cat_D.dds");

	LOG_TIMER(timer, "Textures", debugLog);
	RESET_TIMER(timer);

	// Materials
	materialIndices.try_emplace(Material::Test,      engine.CreateMaterial(1024 * 64, sizeof(Vertex), { memeTexture }, defaultShader));
	materialIndices.try_emplace(Material::Ground,    engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, groundShader));
	materialIndices.try_emplace(Material::Laser,     engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, laserShader));
	materialIndices.try_emplace(Material::Portal1,   engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &engine.m_renderTextures[0]->texture}, portalShader));
	materialIndices.try_emplace(Material::Portal2,   engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &engine.m_renderTextures[1]->texture}, portalShader));
	materialIndices.try_emplace(Material::Crosshair, engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, crosshairShader));

	LOG_TIMER(timer, "Materials", debugLog);
	RESET_TIMER(timer);

	// Meshes
	// TODO: why does mesh need material index, and why doesn't it matter if it's wrong?
	MeshFile cubeMeshFile = LoadGltfFromFile("models/cube.glb", debugLog, globalArena).meshes[0];
	auto cubeMeshView = engine.CreateMesh(materialIndices[Material::Test], cubeMeshFile.vertices, cubeMeshFile.vertexCount, 0);
	engine.cubeVertexView = cubeMeshView;

	LOG_TIMER(timer, "Default Meshes", debugLog);
	RESET_TIMER(timer);

	// Defaults
	playerPitch = XM_PI;

	light.position = { 10.f, 10.f, 10.f };

	LOG_TIMER(timer, "Other Entities", debugLog);
	RESET_TIMER(timer);

	// Audio
	soundFiles[(size_t)AudioFile::PlayerDamage] = LoadAudioFile(L"audio/chord.wav", globalArena);
	soundFiles[(size_t)AudioFile::EnemyDeath] = LoadAudioFile(L"audio/tada.wav", globalArena);
	soundFiles[(size_t)AudioFile::Shoot] = LoadAudioFile(L"audio/laser.wav", globalArena);
	
	// Physics
	collisionConfiguration = NewObject(globalArena, btDefaultCollisionConfiguration);
	dispatcher = NewObject(globalArena, btCollisionDispatcher, collisionConfiguration);
	broadphase = NewObject(globalArena, btDbvtBroadphase);
	solver = NewObject(globalArena, btSequentialImpulseConstraintSolver);
	physicsDebug.log = &debugLog;
	physicsDebug.engine = &engine;

	// Finish setup
	LoadLevel(engine);
	engine.UploadVertices();
	UpdateCursorState();

	LOG_TIMER(timer, "Finalize", debugLog);
	RESET_TIMER(timer);
}

void Game::LoadLevel(EngineCore& engine)
{
	levelLoaded = true;

	// TODO: this arena should be reset in the engine
	entityArena.Reset();

	// Physics
	dynamicsWorld = NewObject(levelArena, btDiscreteDynamicsWorld, dispatcher, broadphase, solver, collisionConfiguration);
	dynamicsWorld->setGravity(btVector3(0.f, -10.f, 0.f));
	dynamicsWorld->setDebugDrawer(&physicsDebug);

	// Portals
	auto createPortal = [&](size_t matIndex, XMVECTOR pos, const char* name) {
		Entity* portalRenderQuad = CreateQuadEntity(engine, matIndex, 2.f, 4.f);
		portalRenderQuad->SetLocalPosition({ -1.f, 2.f, 0.f });
		portalRenderQuad->SetLocalRotation(XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0.f, 0.f));
		portalRenderQuad->name = "PortalQuad";

		Entity* portal = CreateEmptyEntity(engine);
		portal->SetLocalPosition(pos);
		portal->name = name;

		portal->AddChild(portalRenderQuad, false);

		return portal;
	};
	portal1 = createPortal(materialIndices[Material::Portal1], { 0.f, 2.f, 10.f }, "Portal 1");
	portal2 = createPortal(materialIndices[Material::Portal2], { 2.f, 2.f, 10.f }, "Portal 2");

	// Gizmo
	gizmo = LoadGizmo(engine, *this, materialIndices[Material::Laser]);

	// Crosshair
	Entity* crosshair = CreateQuadEntity(engine, materialIndices[Material::Crosshair], .03f, .03f, true);
	crosshair->GetData().mainOnly = true;
	crosshair->GetBuffer().color = { 1.f, .3f, .1f, 1.f };

	// Player
	playerEntity = CreateEmptyEntity(engine);
	playerEntity->name = "Player";
	playerEntity->SetLocalPosition({ 0.f, 1.f, 0.f });

	btBoxShape* playerPhysicsShape = NewObject(levelArena, btBoxShape, ToBulletVec3({ .5f, 1.f, .5f }));
	playerEntity->physicsShapeOffset = { 0.f, 1.f, 0.f };
	PhysicsInit playerPhysics{ 10.f, PhysicsInitType::RigidBodyDynamic };
	playerEntity->AddRigidBody(levelArena, dynamicsWorld, playerPhysicsShape, playerPhysics);

	playerLookEntity = CreateEmptyEntity(engine);
	playerLookEntity->name = "PlayerLook";
	playerLookEntity->SetLocalPosition(defaultPlayerLookPosition);
	playerEntity->AddChild(playerLookEntity, false);

	cameraEntity = CreateEmptyEntity(engine);
	cameraEntity->name = "Camera";
	cameraEntity->SetLocalPosition({ 0.f, 0.f, 0.15f });
	playerLookEntity->AddChild(cameraEntity, false);

	playerModelEntity = CreateEntityFromGltf(engine, "models/kaiju.glb", L"entity", debugLog);

	assert(playerModelEntity->isSkinnedRoot);
	playerModelEntity->name = "KaijuRoot";
	playerModelEntity->transformHierachy->SetAnimationActive("BasePose", true);
	playerModelEntity->transformHierachy->SetAnimationActive("NeckShrink", true);

	playerEntity->AddChild(playerModelEntity, false);

	// Ground
	PhysicsInit groundPhysics{ 0.f, PhysicsInitType::RigidBodyStatic };
	Entity* groundEntity = CreateQuadEntity(engine, materialIndices[Material::Ground], 100.f, 100.f, groundPhysics);
	groundEntity->name = "Ground";
	groundEntity->GetBuffer().color = { 1.f, 1.f, 1.f };
	groundEntity->SetLocalPosition(XMVectorSetY(groundEntity->localMatrix.translation, -.01f));

	for (int i = 0; i < 16; i++)
	{
		Entity* yea = CreateMeshEntity(engine, materialIndices[Material::Ground], engine.cubeVertexView);
		yea->name = "Yea";
		yea->SetLocalPosition({ 3.f, 0.5f + i * 16.f, 0.f });
		btBoxShape* boxShape = NewObject(levelArena, btBoxShape, ToBulletVec3({ .5f, .5f, .5f }));
		PhysicsInit yeaPhysics{ 1.f, PhysicsInitType::RigidBodyDynamic };
		yea->AddRigidBody(levelArena, dynamicsWorld, boxShape, yeaPhysics);
	}
}

void Game::UpdateGame(EngineCore& engine)
{
	input.accessMutex.lock();
	if (frameStep)
	{
		DrawUI(engine);

		if (input.KeyJustPressed(VK_TAB))
		{
			debugLog.Log("Step...");
		}
		else
		{
			input.accessMutex.unlock();
			return;
		}
	}
	input.accessMutex.unlock();

	engine.BeginProfile("Game Update", ImColor::HSV(.75f, 1.f, 1.f));

	if (!levelLoaded)
	{
		LoadLevel(engine);
	}

	input.accessMutex.lock();
	input.UpdateMousePosition();

	// Reset per frame values
	engine.m_debugLineData.lineVertices.clear();

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
	if (input.KeyComboJustPressed(VK_KEY_N, VK_CONTROL))
	{
		ToggleNoclip();
	}
	if (input.KeyComboJustPressed(VK_TAB, VK_CONTROL))
	{
		frameStep = !frameStep;
		if (frameStep) debugLog.Log("Frame Step Enabled");
		else debugLog.Log("Frame Step Disabled");
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

	cameraEntity->SetLocalRotation(XMQuaternionRotationRollPitchYaw(playerPitch, 0.f, 0.f));
	playerLookEntity->SetLocalRotation(XMQuaternionRotationRollPitchYaw(0.f, playerYaw, 0.f));

	XMVECTOR horizontalCamForward = XMVectorSetY(cameraEntity->worldMatrix.forward, 0.f);
	if (XMVectorGetX(XMVector3AngleBetweenVectors(horizontalCamForward, playerModelEntity->GetForwardDirection())) > XMConvertToRadians(20.f))
	{
		playerModelEntity->SetForwardDirection(XMVector3Normalize(horizontalCamForward));
	}

	if (input.KeyComboJustPressed(VK_KEY_R, VK_CONTROL))
	{
		engine.m_resetLevel = true;
		levelLoaded = false;
	}

	// Player movement
	float horizontalInput = 0.f;
	if (input.KeyDown(VK_KEY_A)) horizontalInput -= 1.f;
	if (input.KeyDown(VK_KEY_D)) horizontalInput += 1.f;

	float verticalInput = 0.f;
	if (input.KeyDown(VK_KEY_S)) verticalInput -= 1.f;
	if (input.KeyDown(VK_KEY_W)) verticalInput += 1.f;

	if (noclip)
	{
		float camSpeed = 10.f;
		if (input.KeyDown(VK_SHIFT)) camSpeed *= .1f;
		if (input.KeyDown(VK_CONTROL)) camSpeed *= 5.f;

		XMVECTOR lookPos = playerLookEntity->GetWorldPosition();
		lookPos += cameraEntity->worldMatrix.right * horizontalInput * engine.m_updateDeltaTime * camSpeed;
		lookPos += cameraEntity->worldMatrix.forward * verticalInput * engine.m_updateDeltaTime * camSpeed;
		playerLookEntity->SetWorldPosition(lookPos);
	}
	else
	{
		// Buffer jump for later
		if (input.KeyJustPressed(VK_SPACE))
		{
			lastJumpPressTime = engine.TimeSinceStart();
		}

		// Add input to velocity
		XMVECTOR playerForward = XMVector3Normalize(XMVectorSetY(cameraEntity->worldMatrix.forward, 0.f));
		XMVECTOR playerRight = XMVector3Normalize(XMVectorSetY(cameraEntity->worldMatrix.right, 0.f));

		XMVECTOR wantedForward = XMVectorSetY(XMVectorScale(playerForward, verticalInput), 0.f);
		XMVECTOR wantedSideways = XMVectorSetY(XMVectorScale(playerRight, horizontalInput), 0.f);
		XMVECTOR wantedDirection = XMVector3Normalize(wantedForward + wantedSideways);
		bool playerWantsDirection = XMVectorGetX(XMVector3LengthSq(wantedForward) + XMVector3LengthSq(wantedSideways)) > 0.01f;

		XMVECTOR playerVelocity = ToXMVec(playerEntity->rigidBody->getLinearVelocity());
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

			horizontalPlayerSpeed = horizontalPlayerSpeed * resultScale + movementSettings->playerAcceleration * engine.m_updateDeltaTime;
			if (horizontalPlayerSpeed > movementSettings->playerMaxSpeed)
			{
				horizontalPlayerSpeed = movementSettings->playerMaxSpeed;
			}

			playerVelocity = XMVectorScale(resultDirection, horizontalPlayerSpeed);
			playerVelocity = XMVectorSetY(playerVelocity, verticalPlayerVelocity);
		}

		// Ground collision
		const float maxPlayerSpeedEstimate = std::max(10.f, playerEntity->rigidBody->getLinearVelocity().y());
		const float collisionEpsilon = maxPlayerSpeedEstimate * engine.m_updateDeltaTime;
		XMVECTOR groundRayStart = playerEntity->GetWorldPosition() + V3_UP * collisionEpsilon;
		XMVECTOR groundRayEnd = playerEntity->GetWorldPosition() - V3_UP * collisionEpsilon;

		btCollisionWorld::ClosestRayResultCallback callback{ ToBulletVec3(groundRayStart), ToBulletVec3(groundRayEnd) };
		dynamicsWorld->rayTest(ToBulletVec3(groundRayStart), ToBulletVec3(groundRayEnd), callback);
		playerOnGround = callback.hasHit();
		if (callback.hasHit()) debugLog.Log("Ray hit: {} {} {}", SPLIT_V3_BT(callback.m_hitPointWorld));
		
		if (playerOnGround)
		{
			// Move player on ground
			playerEntity->SetWorldPosition(ToXMVec(callback.m_hitPointWorld));

			// Stop falling speed
			playerVelocity = XMVectorSetY(playerVelocity, 0.);

			// Apply jump
			if (input.KeyDown(VK_SPACE) && (lastJumpPressTime + jumpBufferDuration >= engine.TimeSinceStart() || movementSettings->autojump))
			{
				lastJumpPressTime = -1000.f;
				playerVelocity.m128_f32[1] += movementSettings->playerJumpStrength;
			}
			else
			{
				// Apply friction when no input
				if (std::abs(horizontalInput) <= inputDeadzone && std::abs(verticalInput) <= inputDeadzone)
				{
					float speedDecrease = movementSettings->playerFriction * engine.m_updateDeltaTime;
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
			playerVelocity.m128_f32[1] -= engine.m_updateDeltaTime * movementSettings->playerGravity;
		}

		// Apply velocity
		playerEntity->rigidBody->setLinearVelocity(ToBulletVec3(playerVelocity));
	}

	// Gizmo
	if (editMode && editElement != nullptr && input.KeyJustPressed(VK_LBUTTON))
	{
		/*RaycastScreenPosition(engine, *engine.mainCamera, {input.mouseX, input.mouseY}, &allRaycastCollector, CollisionLayers::GizmoClick);
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
		}*/
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
	}
	
	if (input.KeyJustReleased(VK_LBUTTON))
	{
		selectedGizmoElement = nullptr;
		selectedGizmoTarget = nullptr;
	}

	// Player Audio
	XMStoreFloat3(&playerAudioListener.OrientFront, cameraEntity->worldMatrix.forward);
	XMStoreFloat3(&playerAudioListener.OrientTop, cameraEntity->worldMatrix.up);
	XMStoreFloat3(&playerAudioListener.Position, engine.mainCamera->worldMatrix.translation);
	if (playerEntity->rigidBody != nullptr) XMStoreFloat3(&playerAudioListener.Velocity, ToXMVec(playerEntity->rigidBody->getLinearVelocity()));

	// Shoot portal
	if (input.KeyJustPressed(VK_LBUTTON) || input.KeyJustPressed(VK_RBUTTON))
	{
		/*minRaycastCollector.Raycast(physicsWorld, engine.mainCamera->worldMatrix.translation, engine.mainCamera->worldMatrix.translation + cameraEntity->worldMatrix.forward * 1000.f, CollisionLayers::Floor);
		if (minRaycastCollector.anyCollision)
		{
			Entity* targetPortal = input.KeyJustPressed(VK_LBUTTON) ? portal1 : portal2;
			targetPortal->SetWorldPosition(minRaycastCollector.collision.worldPoint - cameraEntity->worldMatrix.forward * 0.001f);
			targetPortal->SetForwardDirection(minRaycastCollector.collision.worldNormal);
			PlaySound(engine, &playerAudioSource, AudioFile::Shoot);
		}*/
	}

	// Update animations: iteration order is not guaranteed to be parent->child, so do other things in a separate pass
	for (Entity& entity : entityArena)
	{
		entity.UpdateAnimation(engine, false);
	}

	// Apply portal transition
	for (Entity& entity : entityArena)
	{
		if (entity.isNearPortal1)
		{
			XMVECTOR dot = XMVector3Dot(portal1->GetForwardDirection(), XMVector3Normalize(entity.GetWorldPosition() - portal1->GetWorldPosition()));
			if (XMVectorGetX(dot) < 0.f)
			{
				debugLog.Log("prtl1");
			}
		}
		if (entity.isNearPortal2)
		{
			XMVECTOR dot = XMVector3Dot(portal2->GetForwardDirection(), XMVector3Normalize(entity.GetWorldPosition() - portal2->GetWorldPosition()));
			if (XMVectorGetX(dot) < 0.f)
			{
				debugLog.Log("prtl2");
			}
		}
	}

	for (Entity& entity : entityArena)
	{
		//entity.WritePhysicsTransform();
	}

	dynamicsWorld->stepSimulation(engine.m_updateDeltaTime, 3, MAX_PHYSICS_STEP);

	if (renderPhysics)
	{
		physicsDebug.setDebugMode(btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawContactPoints);
	}
	else
	{
		physicsDebug.setDebugMode(btIDebugDraw::DBG_NoDebug);
	}
	dynamicsWorld->debugDrawWorld();
	
	if (renderPhysics)
	{
		/*reactphysics3d::DebugRenderer& debugRenderer = physicsWorld->getDebugRenderer();
		for (int i = 0; i < debugRenderer.getNbLines(); i++)
		{
			auto line = debugRenderer.getLinesArray()[i];
			engine.m_debugLineData.AddLine(XMVectorFromPhysics(line.point1), XMVectorFromPhysics(line.point2), XMColorFromPhysics(line.color1), XMColorFromPhysics(line.color2));
		}*/
	}

	for (Entity& entity : entityArena)
	{
		//entity.ReadPhysicsTransform();
		entity.UpdateAudio(engine, &playerAudioListener);
	}

	// Update Camera
	engine.mainCamera->UpdateViewMatrix(cameraEntity->worldMatrix.matrix);
	engine.mainCamera->UpdateProjectionMatrix();

	XMVECTOR t1;
	XMVECTOR r1;
	XMVECTOR s1;
	XMMatrixDecompose(&s1, &r1, &t1, portal1->worldMatrix.inverse);

	XMVECTOR t2;
	XMVECTOR r2;
	XMVECTOR s2;
	XMMatrixDecompose(&s2, &r2, &t2, portal2->worldMatrix.inverse);

	MAT_RMAJ portal1Mat = cameraEntity->worldMatrix.matrix * portal2->worldMatrix.inverse * portal1->worldMatrix.matrix;
	MAT_RMAJ portal2Mat = cameraEntity->worldMatrix.matrix * portal1->worldMatrix.inverse * portal2->worldMatrix.matrix;

	engine.m_renderTextures[0]->camera->UpdateViewMatrix(portal1Mat);
	engine.m_renderTextures[0]->camera->UpdateProjectionMatrix();

	engine.m_renderTextures[1]->camera->UpdateViewMatrix(portal2Mat);
	engine.m_renderTextures[1]->camera->UpdateProjectionMatrix();

	for (CameraData& camera : engine.m_cameras)
	{
		CameraConstantBuffer& cambuf = camera.constantBuffer.data;
		cambuf.postProcessing = { contrast, brightness, saturation, fog };
		cambuf.fogColor = clearColor;
	}

	// Update light/shadowmap
	light.rotation = XMQuaternionRotationRollPitchYaw(45.f / 360.f * XM_2PI, engine.TimeSinceStart(), 0.f);

	MAT_RMAJ lightView = XMMatrixMultiply(XMMatrixTranslationFromVector(-light.position), XMMatrixRotationQuaternion(XMQuaternionInverse(light.rotation)));
	engine.m_lightConstantBuffer.data.lightView = XMMatrixTranspose(lightView);

	if (showLightPosition)
	{
		engine.m_debugLineData.AddLine(light.position, light.position + XMVector3TransformNormal({ 0.f, 0.f, 1.f }, XMMatrixRotationQuaternion(light.rotation)), { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });
	}

	engine.m_lightConstantBuffer.data.sunDirection = XMVector3TransformNormal({ 0.f, 0.f, 1.f }, XMMatrixRotationQuaternion(light.rotation));

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
			engine.m_debugLineData.lineVertices.newElement() = { edgeStart3, cubeColor };
			engine.m_debugLineData.lineVertices.newElement() = { edgeEnd3, cubeColor };
		}
	}

	// Post Processing
	XMVECTOR camPos2d = engine.mainCamera->worldMatrix.translation;
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

Entity* Game::CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, bool vertical)
{
	MeshFile file = vertical ? CreateQuadY(width, height, globalArena) : CreateQuad(width, height, globalArena);
	auto meshView = engine.CreateMesh(materialIndex, file.vertices, file.vertexCount, file.meshHash);
	Entity* entity = CreateMeshEntity(engine, materialIndex, meshView);
	entity->SetLocalPosition({-width / 2.f, 0.f, -height / 2.f});

	return entity;
}

Entity* Game::CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, PhysicsInit& physicsInit, bool vertical)
{
	Entity* entity = CreateQuadEntity(engine, materialIndex, width, height, vertical);

	if (physicsInit.type != PhysicsInitType::None)
	{
		btBoxShape* physicsShape = NewObject(levelArena, btBoxShape, ToBulletVec3({ width / 2.f, 0.05f, height / 2.f }));
		entity->physicsShapeOffset = XMVECTOR{ width / 2.f, -.05f, height / 2.f };
		entity->AddRigidBody(levelArena, dynamicsWorld, physicsShape, physicsInit);
	}

	return entity;
}

Entity* Game::CreateEntityFromGltf(EngineCore& engine, const char* path, const std::wstring& shaderName, RingLog& log)
{
	Entity* mainEntity = CreateEmptyEntity(engine);

	GltfResult gltfResult = LoadGltfFromFile(path, log, levelArena);
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
		D3D12_VERTEX_BUFFER_VIEW meshView = engine.CreateMesh(materialIndex, meshFile.vertices, meshFile.vertexCount, meshFile.meshHash);
		LOG_TIMER(timer, "Create material and mesh for model", debugLog);
		RESET_TIMER(timer);

		Entity* child = CreateMeshEntity(engine, materialIndex, meshView);
		child->name = meshFile.textureName.c_str();
		if (gltfResult.transformHierachy != nullptr) child->isSkinnedMesh = true;
		mainEntity->AddChild(child, false);

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

/*void Game::RaycastScreenPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos, EngineRaycastCallback* callback, CollisionLayers layers)
{
	XMVECTOR rayOriginWorld = ScreenToWorldPosition(engine, cameraData, screenPos);
	XMVECTOR rayDirection = XMVector3Normalize(rayOriginWorld - cameraData.worldMatrix.translation);
	callback->Raycast(physicsWorld, rayOriginWorld, rayOriginWorld + rayDirection * 1000.f);
}*/

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

bool Game::LoadGameConfig()
{
	configArena.Reset();
	if (!LoadConfig(configArena, CONFIG_PATH)) return false;

	ConfigFile* configFile = LoadConfigEntry<ConfigFile>(configArena, 0, CONFIG_VERSION);
	if (configFile == nullptr) return false;

	movementSettings = LoadConfigEntry<MovementSettings>(configArena, configFile->movementSettingsOffset, MOVEMENT_SETTINGS_VERSION);
	if (movementSettings == nullptr) return false;
}

void Game::ResetGameConfig()
{
	configArena.Reset();
	ConfigFile* configFile = NewObject(configArena, ConfigFile);

	configFile->movementSettingsOffset = configArena.used;
	movementSettings = NewObject(configArena, MovementSettings);
}

void Game::ToggleNoclip()
{
	noclip = !noclip;
	if (noclip)
	{
		//playerEntity->rigidBody->setLinearVelocity({});
		playerEntity->RemoveChild(playerLookEntity, true);
	}
	else
	{
		playerEntity->AddChild(playerLookEntity, true);
		playerLookEntity->SetLocalPosition(defaultPlayerLookPosition);
	}
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

IGame* CreateGame(GAME_CREATION_PARAMS)
{
	Game* game = NewObject(globalArena, Game, globalArena, configArena, levelArena);
	return game;
}