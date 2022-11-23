#include "Game.h"

#include <array>
#include <format>
#include <limits>

#include "../core/vkcodes.h"
#include "remixicon.h"

void CreateWorldMatrix(XMMATRIX& out, const XMVECTOR scale, const XMVECTOR rotation, const XMVECTOR position)
{
	out = DirectX::XMMatrixTranspose(DirectX::XMMatrixAffineTransformation(scale, XMVECTOR{}, rotation, position));
}

void CalculateDirectionVectors(XMVECTOR& outForward, XMVECTOR& outRight, XMVECTOR inRotation)
{
	XMMATRIX camRotation = XMMatrixRotationQuaternion(inRotation);
	XMVECTOR right{ 1, 0, 0 };
	XMVECTOR forward{ 0, 0, 1 };
	outForward = XMVector3Transform(forward, camRotation);
	outRight = XMVector3Transform(right, camRotation);
}

CollisionResult Game::CollideWithWorld(const XMVECTOR rayOrigin, const XMVECTOR rayDirection, uint64_t matchingLayers)
{
	CollisionResult result{ nullptr, std::numeric_limits<float>::max() };

	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		if ((entity->collisionLayers & matchingLayers) != 0)
		{
			EntityData* entityData = entity->GetData();

			XMVECTOR entityCenterWorld = entity->position + entity->LocalToWorld(entityData->aabbLocalPosition);
			XMVECTOR entitySizeWorld = entity->LocalToWorld(entityData->aabbLocalSize);

			XMVECTOR boxMin = entityCenterWorld - entitySizeWorld / 2.f;
			XMVECTOR boxMax = entityCenterWorld + entitySizeWorld / 2.f;

			XMVECTOR t0 = XMVectorDivide(XMVectorSubtract(boxMin, rayOrigin), rayDirection);
			XMVECTOR t1 = XMVectorDivide(XMVectorSubtract(boxMax, rayOrigin), rayDirection);
			XMVECTOR tmin = XMVectorMin(t0, t1);
			XMVECTOR tmax = XMVectorMax(t0, t1);

			XMFLOAT3 tMinValues;
			XMFLOAT3 tMaxValues;
			XMStoreFloat3(&tMinValues, tmin);
			XMStoreFloat3(&tMaxValues, tmax);

			float minDistance = std::max(std::max(tMinValues.x, tMinValues.y), tMinValues.z);
			float maxDistance = std::min(std::min(tMaxValues.x, tMaxValues.y), tMaxValues.z);
			if (minDistance <= maxDistance && minDistance < result.distance)
			{
				result.distance = minDistance;
				result.entity = entity;
			}
		}
	}

	return result;
}

void Game::StartGame(EngineCore& engine)
{
	// Shaders
	ShaderDescription defaultShader{ L"entity.hlsl", "VSMain", "PSMain", L"Main" };
	ShaderDescription groundShader{ L"ground.hlsl", "VSMain", "PSMain", L"Ground" };

	// Textures
	engine.CreateTexture(diffuseTexture, L"textures/ground-diffuse-bc1.dds", L"Diffuse Texture");
	engine.CreateTexture(memeTexture, L"textures/cat.dds", L"yea");
	engine.CreateTexture(kaijuTexture, L"textures/kaiju.dds", L"kaiju");

	// Materials
	size_t dirtMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &diffuseTexture }, defaultShader);
	size_t memeMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &memeTexture }, defaultShader);
	size_t kaijuMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &kaijuTexture }, defaultShader);
	size_t groundMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, groundShader);

	// Meshes
	MeshFile cubeMeshFile = LoadMeshFromFile("models/cube.obj", "models/", debugLog, vertexUploadArena, indexUploadArena);
	// TODO: why does mesh need material index, and why doesn't it matter if it's wrong?
	auto cubeMeshView = engine.CreateMesh(memeMaterialIndex, cubeMeshFile.vertices, cubeMeshFile.vertexCount);
	engine.cubeVertexView = cubeMeshView;

	MeshFile kaijuMeshFile = LoadMeshFromFile("models/kaiju.obj", "models/", debugLog, vertexUploadArena, indexUploadArena);
	auto kaijuMeshView = engine.CreateMesh(kaijuMaterialIndex, kaijuMeshFile.vertices, kaijuMeshFile.vertexCount);

	// Entities
	Entity* kaijuEntity = CreateEntity(engine, kaijuMaterialIndex, kaijuMeshView);
	kaijuEntity->GetBuffer()->color = { 1.f, 1.f, 1.f };
	kaijuEntity->GetData()->aabbLocalPosition = { 0.f, 5.f, 0.f };
	kaijuEntity->GetData()->aabbLocalSize = { 4.f, 10.f, 2.f };

	for (int i = 0; i < MAX_ENENMY_COUNT; i++)
	{
		Entity* enemy = enemies[i] = CreateEntity(engine, memeMaterialIndex, cubeMeshView);
		enemy->isEnemy = true;
		enemy->isActive = false;
		enemy->collisionLayers |= Dead;
		enemy->checkForShadowBounds = false;
		enemy->GetData()->visible = false;
	}

	for (int i = 0; i < MAX_PROJECTILE_COUNT; i++)
	{
		Entity* projectile = projectiles[i] = CreateEntity(engine, groundMaterialIndex, cubeMeshView);
		projectile->isProjectile = true;
		projectile->isActive = false;
		projectile->GetData()->visible = false;
		projectile->scale = { .1f, .1f, .1f };
	}

	Entity* groundEntity = CreateQuadEntity(engine, groundMaterialIndex, 100.f, 100.f);
	groundEntity->GetBuffer()->color = { 1.f, 1.f, 1.f };
	groundEntity->checkForShadowBounds = false;
	groundEntity->collisionLayers |= Floor;

	// Defaults
	camera.position = { 0.f, 10.f, 0.f };
	playerPitch = XM_PI;

	light.position = { 10.f, 10.f, 10.f };

	// Finish setup
	engine.UploadVertices();
	UpdateCursorState();
}

void Game::UpdateGame(EngineCore& engine)
{
	engine.BeginProfile("Game Update", ImColor::HSV(.75f, 1.f, 1.f));
	input.accessMutex.lock();
	input.UpdateMousePosition();

	// Read camera rotation into vectors
	XMVECTOR camForward;
	XMVECTOR camRight;
	CalculateDirectionVectors(camForward, camRight, camera.rotation);

	// Reset per frame values
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		entity->GetBuffer()->isSelected = { 0 };
	}

	// Debug Controls
	if (input.KeyJustPressed(VK_ESCAPE))
	{
		showEscMenu = !showEscMenu;
		UpdateCursorState();
	}
	if (input.KeyDown(VK_LBUTTON))
	{
		CollisionResult collision = CollideWithWorld(camera.position, camForward, ClickTest);
		if (collision.entity != nullptr)
		{
			collision.entity->GetBuffer()->isSelected = { 1 };
		}
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

	if (!showEscMenu)
	{
		const float maxPitch = XM_PI * 0.49;

		playerYaw += input.mouseDeltaX * 0.003f;
		playerPitch += input.mouseDeltaY * 0.003f;
		if (playerPitch > maxPitch) playerPitch = maxPitch;
		if (playerPitch < -maxPitch) playerPitch = -maxPitch;
	}

	// Camera controls
	if (input.KeyComboJustPressed(VK_KEY_R, VK_CONTROL))
	{
		camera.position = { 0.f, 0.f, 0.f };
	}

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

		camera.position += camRight * horizontalInput * engine.m_updateDeltaTime * camSpeed;
		camera.position += camForward * verticalInput * engine.m_updateDeltaTime * camSpeed;
	}
	else
	{
		// Buffer jump for later
		if (input.KeyJustPressed(VK_SPACE))
		{
			lastJumpPressTime = engine.TimeSinceStart();
		}

		// Add input to velocity
		XMVECTOR playerForward = camForward;
		playerForward.m128_f32[1] = 0.f;
		playerForward = XMVector3Normalize(playerForward);

		XMVECTOR playerRight = camRight;
		playerRight.m128_f32[1] = 0.f;
		playerRight = XMVector3Normalize(playerRight);

		playerVelocity += XMVectorScale(playerForward, verticalInput * engine.m_updateDeltaTime * playerAcceleration);
		playerVelocity += XMVectorScale(playerRight, horizontalInput * engine.m_updateDeltaTime * playerAcceleration);

		// Limit speed
		float verticalPlayerVelocity = playerVelocity.m128_f32[1];
		XMVECTOR horizontalPlayerVelocity = playerVelocity;
		horizontalPlayerVelocity.m128_f32[1] = 0.f;

		float horizontalPlayerSpeed = XMVector3Length(horizontalPlayerVelocity).m128_f32[0];
		if (horizontalPlayerSpeed > playerMaxSpeed)
		{
			horizontalPlayerVelocity = XMVector3Normalize(horizontalPlayerVelocity) * playerMaxSpeed;
			horizontalPlayerSpeed = playerMaxSpeed;

			playerVelocity = horizontalPlayerVelocity;
			playerVelocity.m128_f32[1] = verticalPlayerVelocity;
		}

		// Ground collision
		CollisionResult floorCollision = CollideWithWorld(camera.position, V3_DOWN, Floor);
		bool onGround = floorCollision.distance <= playerHeight;
		if (onGround)
		{
			// Move player out of ground
			XMVECTOR collisionPoint = camera.position + XMVectorScale(V3_DOWN, floorCollision.distance);
			camera.position = collisionPoint - XMVectorScale(V3_DOWN, playerHeight);

			// Stop falling speed
			playerVelocity = horizontalPlayerVelocity;

			// Apply jump
			if (input.KeyDown(VK_SPACE) && lastJumpPressTime + jumpBufferDuration >= engine.TimeSinceStart())
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
		camera.position += playerVelocity * engine.m_updateDeltaTime;
	}

	// Update Camera
	camera.rotation = XMQuaternionMultiply(XMQuaternionRotationRollPitchYaw(playerPitch, 0.f, 0.f), XMQuaternionRotationRollPitchYaw(0.f, playerYaw, 0.f));
	CalculateDirectionVectors(camForward, camRight, camera.rotation);

	engine.m_sceneConstantBuffer.data.cameraView = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(camera.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(camera.rotation)));
	engine.m_sceneConstantBuffer.data.cameraProjection = XMMatrixTranspose(XMMatrixPerspectiveFovLH(camera.fovY, engine.m_aspectRatio, camera.nearClip, camera.farClip));
	engine.m_sceneConstantBuffer.data.postProcessing = { contrast, brightness, saturation, fog };
	engine.m_sceneConstantBuffer.data.worldCameraPos = camera.position;

	// Spawn enemies
	if (engine.TimeSinceStart() >= lastEnemySpawn + enemySpawnRate)
	{
		lastEnemySpawn = engine.TimeSinceStart();

		bool enemySpawned = false;
		for (int i = 0; i < MAX_ENENMY_COUNT; i++)
		{
			Entity* enemy = enemies[i];
			if (!enemy->isActive)
			{
				enemy->isActive = true;
				enemy->GetData()->visible = true;
				enemy->position = {};

				enemySpawned = true;
				break;
			}
		}

		if (!enemySpawned)
		{
			Warn("Too many enemies!");
		}
	}

	// Update enemies
	for (int i = 0; i < MAX_ENENMY_COUNT; i++)
	{
		Entity* enemy = enemies[i];
		if (!enemy->isActive) continue;

		XMVECTOR toPlayer = XMVector3Normalize(camera.position - enemy->position);
		enemy->velocity += XMVectorScale(toPlayer, engine.m_updateDeltaTime * enemyAcceleration);
		float enemySpeed = XMVector3Length(enemy->velocity).m128_f32[0];
		if (enemySpeed > enemyMaxSpeed)
		{
			enemy->velocity = XMVectorScale(XMVector3Normalize(enemy->velocity), enemyMaxSpeed);
		}
		enemy->position += enemy->velocity * engine.m_updateDeltaTime;
	}

	// Kinda hacky enemy collision detection
	CollisionResult enemyCollision = CollideWithWorld(camera.position, V3_DOWN, Dead);
	if (enemyCollision.distance <= 0.f && enemyCollision.entity != nullptr)
	{
		Warn("U R DED!!");
		enemyCollision.entity->isActive = false;
		enemyCollision.entity->GetData()->visible = false;
	}

	// Update Projectiles
	for (int i = 0; i < MAX_PROJECTILE_COUNT; i++)
	{
		Entity* projectile = projectiles[i];
		if (!projectile->isActive) continue;

		if (engine.TimeSinceStart() > projectile->spawnTime + projectileLifetime)
		{
			projectile->isActive = false;
			projectile->GetData()->visible = false;
			continue;
		}

		projectile->position += projectile->velocity * engine.m_updateDeltaTime;

		CollisionResult enemyCollision = CollideWithWorld(projectile->position, V3_DOWN, Dead);
		if (enemyCollision.distance <= 0.f && enemyCollision.entity != nullptr)
		{
			enemyCollision.entity->isActive = false;
			enemyCollision.entity->GetData()->visible = false;
		}
	}

	// Shoot!
	if (input.KeyDown(VK_LBUTTON) && engine.TimeSinceStart() > lastProjectileSpawn + projectileSpawnRate)
	{
		for (int i = 0; i < MAX_PROJECTILE_COUNT; i++)
		{
			Entity* projectile = projectiles[i];
			if (projectile->isActive) continue;

			projectile->isActive = true;
			projectile->spawnTime = engine.TimeSinceStart();
			projectile->position = camera.position + XMVECTOR{ 0.05f, -.2f, .1f };
			projectile->velocity = camForward * projectileSpeed + playerVelocity;
			projectile->GetData()->visible = true;

			lastProjectileSpawn = engine.TimeSinceStart();
			break;
		}
	}

	// Update entities
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		if (entity->isSpinning)
		{
			entity->rotation = XMQuaternionRotationAxis(XMVECTOR{0.f, 1.f, 0.f}, static_cast<float>(engine.TimeSinceStart()));
		}

		entity->GetBuffer()->aabbLocalPosition = entity->GetData()->aabbLocalPosition;
		entity->GetBuffer()->aabbLocalSize = entity->GetData()->aabbLocalSize;

		CreateWorldMatrix(entity->GetBuffer()->worldTransform, entity->scale, entity->rotation, entity->position);
	}

	// Update light/shadowmap
	light.rotation = XMQuaternionRotationRollPitchYaw(45.f / 360.f * XM_2PI, engine.TimeSinceStart(), 0.f);

	engine.m_lightConstantBuffer.data.lightView = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(light.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(light.rotation)));

	XMVECTOR lightSpaceMin{};
	XMVECTOR lightSpaceMax{};
	XMVECTOR testPositionBuffer[8] = {
		{ -1., -1., -1.},
		{ -1., -1.,  1.},
		{ -1.,  1., -1.},
		{ -1.,  1.,  1.},
		{  1., -1., -1.},
		{  1., -1.,  1.},
		{  1.,  1., -1.},
		{  1.,  1.,  1.},
	};
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		if (!entity->checkForShadowBounds) continue;

		XMVECTOR aabbWorldPosition = XMVector3Transform(entity->GetData()->aabbLocalPosition, entity->GetBuffer()->worldTransform);
		XMVECTOR aabbWorldSize = XMVector3Transform(entity->GetData()->aabbLocalSize, entity->GetBuffer()->worldTransform);

		for (int i = 0; i < _countof(testPositionBuffer); i++)
		{
			XMVECTOR aabbWorldCorner = XMVectorMultiply(aabbWorldSize, testPositionBuffer[i]);
			XMVECTOR lightSpacePosition = XMVector3Transform(entity->position + aabbWorldPosition + aabbWorldCorner, engine.m_lightConstantBuffer.data.lightView);
			lightSpaceMin = XMVectorMin(lightSpacePosition, lightSpaceMin);
			lightSpaceMax = XMVectorMax(lightSpacePosition, lightSpaceMax);
		}
	}

	XMVECTOR lightRight;
	CalculateDirectionVectors(engine.m_lightConstantBuffer.data.sunDirection, lightRight, light.rotation);
	XMFLOAT3 lsMin;
	XMFLOAT3 lsMax;
	XMStoreFloat3(&lsMin, lightSpaceMin);
	XMStoreFloat3(&lsMax, lightSpaceMax);

	engine.m_lightConstantBuffer.data.lightProjection = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(lsMin.x, lsMax.x, lsMin.y, lsMax.y, lsMin.z - 10., lsMax.z + 10.));

	// Post Processing
	XMVECTOR baseClearColor = { .1f, .2f, .4f, 1.f };
	XMVECTOR fogColor = { .3f, .3f, .3f, 1.f };
	clearColor = XMVectorLerp(baseClearColor, fogColor, std::min(1.f, std::max(0.f, fog)));

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

float* Game::GetClearColor()
{
	return clearColor.m128_f32;
}

EngineInput& Game::GetInput()
{
	return input;
}

Entity* Game::CreateEntity(EngineCore& engine, size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshView, MemoryArena* arena)
{
	Entity* entity = NewObject(arena == nullptr ? entityArena : *arena, Entity);
	entity->engine = &engine;
	entity->materialIndex = materialIndex;
	entity->dataIndex = engine.CreateEntity(materialIndex, meshView);
	return entity;
}

Entity* Game::CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, MemoryArena* arena)
{
	MeshFile file = CreateQuad(width, height, vertexUploadArena, indexUploadArena);
	auto meshView = engine.CreateMesh(materialIndex, file.vertices, file.vertexCount);
	Entity* entity = CreateEntity(engine, materialIndex, meshView, arena);
	entity->position = { -width / 2.f, 0.f, -width / 2.f };
	entity->GetData()->aabbLocalPosition = { width / 2.f, -.05f, width / 2.f };
	entity->GetData()->aabbLocalSize = { width, .1f, width };
	return entity;
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
