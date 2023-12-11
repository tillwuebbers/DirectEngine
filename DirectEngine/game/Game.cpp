/*
* Main game logic, load and manage entities
*/

#include "Game.h"

#include "../Helpers.h"

#include <array>
#include <format>

#include "../core/vkcodes.h"
#include "remixicon.h"

Game::Game(GAME_CREATION_PARAMS) : globalArena(globalArena), configArena(configArena), levelArena(levelArena) {}

void Game::RegisterLog(EngineLog::RingLog* log)
{
	EngineLog::g_debugLog = log;
}

void Game::StartGame(EngineCore& engine)
{
	INIT_TIMER(timer);

	// UI
	ImGui::SetCurrentContext(engine.m_imgui.imGuiContext);
	LoadUIStyle();

	// Config
	if (!LoadGameConfig())
	{
		ResetGameConfig();
		SaveConfig(configArena, CONFIG_PATH);
	}

	LOG_TIMER(timer, "Config");
	RESET_TIMER(timer);

	// Materials
	LoadMaterials("materials.txt", materials, textures);

	for (TextureFile& textureFile : textures)
	{
		textureFile.textureGPU = engine.CreateTexture(textureFile.texturePath.str);
	}

	for (MaterialFile& materialFile : materials)
	{
		std::vector<TextureGPU*> materialTextures = {};
		std::transform(materialFile.textureFiles.begin(), materialFile.textureFiles.end(), std::back_inserter(materialTextures), [](TextureFile* textureFile) { return textureFile->textureGPU; });
		materialFile.data = engine.CreateMaterial(materialFile.shaderName.str, materialTextures, {});
	}

	defaultMaterial = engine.CreateMaterial("ground");
	portal1Material = engine.CreateMaterial("portal", {&engine.m_renderTextures[0]->texture});
	portal2Material = engine.CreateMaterial("portal", { &engine.m_renderTextures[1]->texture });

	D3D12_RASTERIZER_DESC shellRasterizerDesc = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
	shellRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	MaterialData* shellMaterial = engine.CreateMaterial("shelltexture", {}, {{RootConstantType::FLOAT, "layerOffset"}, {RootConstantType::UINT, "layerCount"}}, shellRasterizerDesc);
	shellMaterial->SetRootConstant(0, 0.005f);
	shellMaterial->SetRootConstant(1, 16);
	shellMaterial->shellCount = 16;

	LOG_TIMER(timer, "Materials");
	RESET_TIMER(timer);

	// Defaults
	playerPitch = XM_PI;

	light.position = { 10.f, 10.f, 10.f };
	light.rotation = XMVector3Normalize({ 0.1f, -0.8f, 0.6f });

	// Audio
	soundFiles[(size_t)AudioFile::PlayerDamage] = LoadAudioFile(L"audio/chord.wav", globalArena);
	soundFiles[(size_t)AudioFile::EnemyDeath] = LoadAudioFile(L"audio/tada.wav", globalArena);
	soundFiles[(size_t)AudioFile::Shoot] = LoadAudioFile(L"audio/laser.wav", globalArena);
	
	// Finish setup
	LoadLevel(engine);
	engine.UploadVertices();
	UpdateCursorState();

	LOG_TIMER(timer, "Finalize");
	RESET_TIMER(timer);
}

void Game::LoadLevel(EngineCore& engine)
{
	levelLoaded = true;

	// TODO: this (all) arena should be in the engine
	entityArena.Reset();

	// Physics
	collisionConfiguration = NewObject(levelArena, btDefaultCollisionConfiguration);
	dispatcher = NewObject(levelArena, btCollisionDispatcher, collisionConfiguration);
	broadphase = NewObject(levelArena, btDbvtBroadphase);
	solver = NewObject(levelArena, btSequentialImpulseConstraintSolver);
	physicsDebug.engine = &engine;

	dynamicsWorld = NewObject(levelArena, btDiscreteDynamicsWorld, dispatcher, broadphase, solver, collisionConfiguration);
	dynamicsWorld->setGravity(btVector3(0.f, -10.f, 0.f));
	dynamicsWorld->setDebugDrawer(&physicsDebug);

	// Gizmo
	gizmo.Init(levelArena, this, engine, defaultMaterial);

	// Scene Stuff
	Entity* logEntity1 = CreateEntityFromGltf(engine, "models/log1.glb");
	logEntity1->SetLocalPosition({ 2.f, 0.f, 0.f });
	logEntity1->SetLocalScale({ 2.0f, 2.0f, 2.0f });
	Entity* logEntity2 = CreateEntityFromGltf(engine, "models/log2.glb");
	logEntity2->SetLocalPosition({ 4.f, 0.f, 0.f });
	logEntity2->SetLocalScale({ 2.0f, 2.0f, 2.0f });
	Entity* helmet = CreateEntityFromGltf(engine, "models/DamagedHelmet.glb");
	helmet->SetLocalPosition({ 0.f, 1.f, 0.f });
	helmet->SetLocalRotation(XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0.f, 0.f));
	Entity* sponza = CreateEntityFromGltf(engine, "models/Sponza.glb");
	sponza->SetLocalScale({ 0.01f, 0.01f, 0.01f });

	// Level Meshes & Collision
	level1MeshDataGPU.clear();

	GltfResult* level1Gltf = LoadGltfFromFile("models/level1.glb", levelArena);
	btTriangleMesh* levelCollisionMesh = NewObject(levelArena, btTriangleMesh, true, false);
	for (MeshFile& meshFile : level1Gltf->meshes)
	{
		MeshData& meshData = meshFile.mesh;
		for (int i = 0; i < meshData.vertexCount / 3; i++)
		{
			levelCollisionMesh->addTriangle(
				ToBulletVec3(meshData.vertices[i * 3 + 0].position),
				ToBulletVec3(meshData.vertices[i * 3 + 1].position),
				ToBulletVec3(meshData.vertices[i * 3 + 2].position)
			);
		}

		level1MeshDataGPU.newElement() = engine.CreateMesh(meshData);
	}
	btTriangleIndexVertexArray* levelMeshInterface = NewObject(levelArena, btTriangleIndexVertexArray);
	levelMeshInterface->addIndexedMesh(levelCollisionMesh->getIndexedMeshArray()[0]);
	levelShape = NewObject(levelArena, btBvhTriangleMeshShape, levelMeshInterface, true);

	// Portals
	auto createPortal = [&](MaterialData* material, XMVECTOR pos, const char* name) {
		Entity* portal = CreateEmptyEntity(engine);
		portal->SetLocalPosition(pos);
		portal->name = name;

		PhysicsInit portalInit{ 0.f, PhysicsInitType::RigidBodyStatic };
		portalInit.ownCollisionLayers = CollisionLayers::CL_Portal;
		portalInit.collidesWithLayers = CollisionLayers::CL_Player;
		btCollisionShape* portalShape = NewObject(levelArena, btBoxShape, btVector3(1.0f, 2.0f, 0.1f));
		portal->AddRigidBody(levelArena, dynamicsWorld, portalShape, portalInit);
		if (portal->rigidBody != nullptr) portal->rigidBody->setCollisionFlags(portal->rigidBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);

		Entity* portalRenderQuad = CreateQuadEntity(engine, material, 2.0f, 4.0f);
		portalRenderQuad->SetLocalRotation(XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0.0f, 0.0f));
		portalRenderQuad->name = "PortalQuad";
		portal->AddChild(portalRenderQuad, false);
		portalRenderQuad->SetLocalPosition({ -1.0f, 2.0f, 0.0f });
		portalRenderQuad->GetData().raytraceVisible = false;

		return portal;
	};

	portal1 = createPortal(portal1Material, { -8.0f, 1.5f, 0.0f }, "Portal 1");
	portal1->SetLocalRotation(XMQuaternionRotationRollPitchYaw(0.0f, XM_PIDIV2, 0.0f));
	portal2 = createPortal(portal2Material, {  8.0f, 1.5f, 0.0f }, "Portal 2");
	portal2->SetLocalRotation(XMQuaternionRotationRollPitchYaw(0.0f, -XM_PIDIV2, 0.0f));

	// Crosshair
	/*Entity* crosshair = CreateQuadEntity(engine, materialIndices[Material::Crosshair], .03f, .03f, true);
	crosshair->GetData().mainOnly = true;
	crosshair->GetBuffer().color = { 1.f, .3f, .1f, 1.f };*/

	// Player
	playerEntity = CreateEmptyEntity(engine);
	playerEntity->name = "Player";
	playerEntity->SetLocalPosition({ 0.f, 1.f, 0.f });

	btBoxShape* playerPhysicsShape = NewObject(levelArena, btBoxShape, btVector3{ .5f, 1.f, .5f });
	playerEntity->physicsShapeOffset = { 0.f, 1.f, 0.f };
	PhysicsInit playerPhysics{ 10.f, PhysicsInitType::RigidBodyDynamic };
	playerPhysics.ownCollisionLayers = CollisionLayers::CL_Player;
	playerPhysics.collidesWithLayers = CollisionLayers::CL_Entity | CollisionLayers::CL_Portal;
	playerEntity->AddRigidBody(levelArena, dynamicsWorld, playerPhysicsShape, playerPhysics);
	if (playerEntity->rigidBody != nullptr)
	{
		playerEntity->rigidBody->setAngularFactor(0.f);
		playerEntity->rigidBody->setActivationState(DISABLE_DEACTIVATION);
	}

	playerLookEntity = CreateEmptyEntity(engine);
	playerLookEntity->name = "PlayerLook";
	playerLookEntity->SetLocalPosition(defaultPlayerLookPosition);
	playerEntity->AddChild(playerLookEntity, false);

	cameraEntity = CreateEmptyEntity(engine);
	cameraEntity->name = "Camera";
	cameraEntity->SetLocalPosition({ 0.f, 0.f, 0.15f });
	playerLookEntity->AddChild(cameraEntity, false);

	playerModelEntity = CreateEntityFromGltf(engine, "models/kaiju.glb");

	assert(playerModelEntity->isSkinnedRoot);
	playerModelEntity->name = "KaijuRoot";
	playerModelEntity->transformHierachy->SetAnimationActive("BasePose", true);
	playerModelEntity->transformHierachy->SetAnimationActive("NeckShrink", true);

	playerEntity->AddChild(playerModelEntity, false);

	// Skybox
	skybox = CreateEntityFromGltf(engine, "models/skybox-cube.glb");
	skybox->name = "Skybox";
	skybox->SetLocalScale({ .75f, .75f, .75f });
	skybox->GetData().raytraceVisible = false;
	cameraEntity->AddChild(skybox, false);

	// Level
	/*PhysicsInit levelPhysics{0.f, PhysicsInitType::RigidBodyStatic};
	levelPhysics.ownCollisionLayers = CollisionLayers::CL_World;
	levelPhysics.collidesWithLayers = CollisionLayers::CL_Player | CollisionLayers::CL_Entity;
	
	bool addedCollision = false; // TODO: ugly
	for (MeshDataGPU* meshData : level1MeshDataGPU)
	{
		Entity* levelPart = CreateMeshEntity(engine, materialIndices[Material::Ground], meshData);
		levelPart->name = "Level1";
		if (!addedCollision)
		{
			levelPart->AddRigidBody(levelArena, dynamicsWorld, levelShape, levelPhysics);
			addedCollision = true;
		}
	}*/

	// Ground
	PhysicsInit groundPhysics{0.f, PhysicsInitType::RigidBodyStatic};
	groundPhysics.ownCollisionLayers = CollisionLayers::CL_World;
	groundPhysics.collidesWithLayers = CollisionLayers::CL_Entity;
	Entity* groundEntity = CreateQuadEntity(engine, defaultMaterial, 200.f, 200.f, groundPhysics);
	groundEntity->name = "Ground";
	groundEntity->GetBuffer().color = { 1.f, 1.f, 1.f };
	groundEntity->SetLocalPosition(XMVectorSetY(groundEntity->localMatrix.translation, -.01f));

	// Cubes
	/*cubeMeshDataGPU = engine.CreateMesh(LoadGltfFromFile("models/cube.glb", levelArena).meshes[0]);
	for (int i = 0; i < 16; i++)
	{
		Entity* yea = CreateMeshEntity(engine, materialIndices[Material::ShellTexture], cubeMeshDataGPU);
		yea->name = "Yea";
		yea->SetLocalPosition({ 3.f, 0.5f + i * 16.f, 0.f });
		btBoxShape* boxShape = NewObject(levelArena, btBoxShape, btVector3{ .5f, .5f, .5f });
		PhysicsInit yeaPhysics{ 1.f, PhysicsInitType::RigidBodyDynamic };
		yeaPhysics.ownCollisionLayers = CollisionLayers::CL_Entity;
		yeaPhysics.collidesWithLayers = CollisionLayers::CL_Player | CollisionLayers::CL_World;
		yea->AddRigidBody(levelArena, dynamicsWorld, boxShape, yeaPhysics);
	}*/
}

void PlayerMovement::Update(EngineInput& input, TimeData& time, Entity* playerEntity, Entity* playerLookEntity, Entity* cameraEntity, btDynamicsWorld* dynamicsWorld, bool frameStep)
{
	playerEntity->rigidBody->clearGravity();

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
		lookPos += cameraEntity->worldMatrix.right * horizontalInput * time.deltaTime * camSpeed;
		lookPos += cameraEntity->worldMatrix.forward * verticalInput * time.deltaTime * camSpeed;
		playerLookEntity->SetWorldPosition(lookPos);
	}
	else
	{
		// Buffer jump for later
		if (input.KeyJustPressed(VK_SPACE))
		{
			lastJumpPressTime = time.timeSinceStart;
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

			horizontalPlayerSpeed = horizontalPlayerSpeed * resultScale + movementSettings->playerAcceleration * time.deltaTime;
			if (horizontalPlayerSpeed > movementSettings->playerMaxSpeed)
			{
				horizontalPlayerSpeed = movementSettings->playerMaxSpeed;
			}

			playerVelocity = XMVectorScale(resultDirection, horizontalPlayerSpeed);
			playerVelocity = XMVectorSetY(playerVelocity, verticalPlayerVelocity);
		}

		// Ground collision
		const float maxPlayerSpeedEstimate = std::max(10.f, playerEntity->rigidBody->getLinearVelocity().y());
		const float collisionEpsilon = maxPlayerSpeedEstimate * time.deltaTime;
		XMVECTOR groundRayStart = playerEntity->GetWorldPosition() + V3_UP * collisionEpsilon;
		XMVECTOR groundRayEnd = playerEntity->GetWorldPosition() - V3_UP * collisionEpsilon;

		btCollisionWorld::ClosestRayResultCallback callback{ ToBulletVec3(groundRayStart), ToBulletVec3(groundRayEnd) };
		dynamicsWorld->rayTest(ToBulletVec3(groundRayStart), ToBulletVec3(groundRayEnd), callback);
		playerOnGround = callback.hasHit();

		if (playerOnGround)
		{
			if (frameStep) LOG("HIT GROUND");
			
			// Apply jump
			if (input.KeyDown(VK_SPACE) && (lastJumpPressTime + jumpBufferDuration >= time.timeSinceStart || movementSettings->autojump))
			{
				lastJumpPressTime = -1000.f;
				float verticalSpeed = XMVectorGetY(playerVelocity);
				playerVelocity = XMVectorSetY(playerVelocity, std::max(verticalSpeed, movementSettings->playerJumpStrength));
			}
			else
			{
				// Stop falling speed
				playerVelocity = XMVectorSetY(playerVelocity, 0.f);

				// Apply friction when no input
				if (std::abs(horizontalInput) <= inputDeadzone && std::abs(verticalInput) <= inputDeadzone)
				{
					float speedDecrease = movementSettings->playerFriction * time.deltaTime;
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
			playerVelocity.m128_f32[1] -= time.deltaTime * movementSettings->playerGravity;
		}

		// Apply velocity
		playerEntity->rigidBody->setLinearVelocity(ToBulletVec3(playerVelocity));
		if (frameStep) LOG("Set velocity to: {:.2f} {:.2f} {:.2f}", SPLIT_V3_BT(playerEntity->rigidBody->getLinearVelocity()));
	}
}

void Game::UpdateGame(EngineCore& engine)
{
	TimeData timeData{};
	timeData.deltaTime = engine.m_updateDeltaTime;
	timeData.timeSinceStart = engine.TimeSinceStart();

	input.accessMutex.lock();
	if (frameStep)
	{
		DrawUI(engine);

		if (input.KeyJustPressed(VK_TAB))
		{
			LOG("Step...");
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
		if (frameStep) LOG("Frame Step Enabled");
		else LOG("Frame Step Disabled");
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

	playerMovement.Update(input, timeData, playerEntity, playerLookEntity, cameraEntity, dynamicsWorld, frameStep);
	gizmo.Update(input);

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
				LOG("prtl1");
			}
		}
		if (entity.isNearPortal2)
		{
			XMVECTOR dot = XMVector3Dot(portal2->GetForwardDirection(), XMVector3Normalize(entity.GetWorldPosition() - portal2->GetWorldPosition()));
			if (XMVectorGetX(dot) < 0.f)
			{
				LOG("prtl2");
			}
		}
	}

	btVector3 beforePos;
	btVector3 beforeVel;
	if (playerEntity->rigidBody != nullptr && frameStep)
	{
		beforePos = playerEntity->rigidBody->getWorldTransform().getOrigin();
		beforeVel = playerEntity->rigidBody->getLinearVelocity();
	}
	dynamicsWorld->stepSimulation(engine.m_updateDeltaTime, 4, MAX_PHYSICS_STEP);
	if (playerEntity->rigidBody != nullptr && frameStep)
	{
		btVector3 afterPos = playerEntity->rigidBody->getWorldTransform().getOrigin();
		btVector3 afterVel = playerEntity->rigidBody->getLinearVelocity();
		LOG("Position ({:.2f} {:.2f} {:.2f}) -> ({:.2f} {:.2f} {:.2f})", SPLIT_V3_BT(beforePos), SPLIT_V3_BT(afterPos));
		LOG("Velocity ({:.2f} {:.2f} {:.2f}) -> ({:.2f} {:.2f} {:.2f})", SPLIT_V3_BT(beforeVel), SPLIT_V3_BT(afterVel));
	}

	if (renderPhysics)
	{
		physicsDebug.setDebugMode(btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawContactPoints);
	}
	else
	{
		physicsDebug.setDebugMode(btIDebugDraw::DBG_NoDebug);
	}
	dynamicsWorld->debugDrawWorld();
	
	for (Entity& entity : entityArena)
	{
		entity.UpdateAudio(engine, &playerAudioListener);
	}

	// Update Camera
	engine.mainCamera->UpdateViewMatrix(cameraEntity->worldMatrix.matrix);
	engine.mainCamera->UpdateProjectionMatrix();

	// Update portals
	MAT_RMAJ portalFlipOffset = XMMatrixRotationAxis({ 0.f, 1.f, 0.f }, XM_PI);
	MAT_RMAJ portalCamMatrix = cameraEntity->worldMatrix.matrix;

	MAT_RMAJ portal1Mat = portalCamMatrix * portal1->worldMatrix.inverse * portalFlipOffset * portal2->worldMatrix.matrix;
	MAT_RMAJ portal2Mat = portalCamMatrix * portal2->worldMatrix.inverse * portalFlipOffset * portal1->worldMatrix.matrix;

	engine.m_renderTextures[0]->camera->UpdateViewMatrix(portal1Mat);
	engine.m_renderTextures[0]->camera->UpdateObliqueProjectionMatrix(portal2->worldMatrix.forward, portal2->GetWorldPosition());
	engine.m_renderTextures[1]->camera->UpdateViewMatrix(portal2Mat);
	engine.m_renderTextures[1]->camera->UpdateObliqueProjectionMatrix(portal1->worldMatrix.forward, portal1->GetWorldPosition());

	for (CameraData& camera : engine.m_cameras)
	{
		CameraConstantBuffer& cambuf = camera.constantBuffer.data;
		cambuf.postProcessing = { contrast, brightness, saturation, exposure };
		cambuf.fogData = fogColor;
		cambuf.fogData = XMVectorSetW(cambuf.fogData, fog);
	}

	// Update light/shadowmap
	//light.rotation = XMQuaternionRotationRollPitchYaw(45.f / 360.f * XM_2PI, engine.TimeSinceStart(), 0.f);

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

MaterialFile* Game::GetMaterialFile(uint64_t materialHash)
{
	for (MaterialFile& material : materials)
	{
		if (material.materialHash == materialHash)
		{
			return &material;
		}
	}

	return nullptr;
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

Entity* Game::CreateMeshEntity(EngineCore& engine, MaterialData* material, MeshDataGPU* meshData)
{
	assert(material != nullptr);
	Entity* entity = NewObject(entityArena, Entity);
	entity->engine = &engine;
	entity->isRendered = true;
	entity->material = material;
	entity->dataIndex = engine.CreateEntity(material, meshData);
	return entity;
}

Entity* Game::CreateQuadEntity(EngineCore& engine, MaterialData* material, float width, float height, bool vertical)
{
	assert(material != nullptr);
	MeshData file = vertical ? CreateQuadY(width, height, levelArena) : CreateQuad(width, height, levelArena);
	MeshDataGPU* meshData = engine.CreateMesh(file);
	Entity* entity = CreateMeshEntity(engine, material, meshData);
	entity->SetLocalPosition({ -width / 2.f, 0.f, -height / 2.f });

	return entity;
}

Entity* Game::CreateQuadEntity(EngineCore& engine, MaterialData* material, float width, float height, PhysicsInit& physicsInit, bool vertical)
{
	assert(material != nullptr);
	Entity* entity = CreateQuadEntity(engine, material, width, height, vertical);

	if (physicsInit.type != PhysicsInitType::None)
	{
		btBoxShape* physicsShape = NewObject(levelArena, btBoxShape, btVector3{ width / 2.f, 0.05f, height / 2.f });
		entity->physicsShapeOffset = XMVECTOR{ width / 2.f, -.05f, height / 2.f };
		entity->AddRigidBody(levelArena, dynamicsWorld, physicsShape, physicsInit);
	}

	return entity;
}

Entity* Game::LoadEntity(EngineCore& engine, MeshFile& meshFile)
{
	MaterialFile* materialFile = GetMaterialFile(meshFile.materialHash);
	MaterialData* material;
	if (materialFile != nullptr)
	{
		std::vector<TextureGPU*> textures = {};
		std::transform(materialFile->textureFiles.begin(), materialFile->textureFiles.end(), std::back_inserter(textures), [](TextureFile* textureFile) { return textureFile->textureGPU; });
		material = engine.CreateMaterial(materialFile->shaderName.str, textures);
	}
	else
	{
		WARN("Material {} not found", meshFile.materialName.str);
		material = defaultMaterial;
	}

	MeshDataGPU* meshData = engine.CreateMesh(meshFile.mesh);
	Entity* entity = CreateMeshEntity(engine, material, meshData);
	entity->name = meshFile.materialName.str;
	return entity;
}

Entity* Game::CreateEntityFromGltf(EngineCore& engine, const char* path)
{
	GltfResult* gltfResult = LoadGltfFromFile(path, levelArena);
	if (!gltfResult->success)
	{
		WARN("Failed to load glTF {}", path);
		Entity* mainEntity = CreateEmptyEntity(engine);
		mainEntity->name = "empty";
		return mainEntity;
	}

	if (gltfResult->meshes.size == 0)
	{
		WARN("No meshes in glTF {}", path);
		Entity* mainEntity = CreateEmptyEntity(engine);
		mainEntity->name = "empty";
		return mainEntity;
	}
	else if (gltfResult->meshes.size == 1)
	{
		MeshFile& meshFile = gltfResult->meshes[0];
		return LoadEntity(engine, meshFile);
	}
	else
	{
		Entity* mainEntity = CreateEmptyEntity(engine);

		if (gltfResult->transformHierachy != nullptr)
		{
			mainEntity->isSkinnedRoot = true;
			mainEntity->transformHierachy = gltfResult->transformHierachy;
		}

		for (MeshFile& meshFile : gltfResult->meshes)
		{
			Entity* child = LoadEntity(engine, meshFile);
			if (gltfResult->transformHierachy != nullptr) child->isSkinnedMesh = true;
			mainEntity->AddChild(child, false);
		}

		return mainEntity;
	}
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

bool Game::LoadGameConfig()
{
	configArena.Reset();
	if (!LoadConfig(configArena, CONFIG_PATH)) return false;

	ConfigFile* configFile = LoadConfigEntry<ConfigFile>(configArena, 0, CONFIG_VERSION);
	if (configFile == nullptr) return false;

	playerMovement.movementSettings = LoadConfigEntry<MovementSettings>(configArena, configFile->movementSettingsOffset, MOVEMENT_SETTINGS_VERSION);
	if (playerMovement.movementSettings == nullptr) return false;

	return true;
}

void Game::ResetGameConfig()
{
	configArena.Reset();
	ConfigFile* configFile = NewObject(configArena, ConfigFile);

	configFile->movementSettingsOffset = configArena.used;
	playerMovement.movementSettings = NewObject(configArena, MovementSettings);
}

void Game::ToggleNoclip()
{
	noclip = !noclip;
	if (noclip)
	{
		playerEntity->rigidBody->setLinearVelocity(btVector3{});
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