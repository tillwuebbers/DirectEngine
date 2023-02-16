#include "Game.h"

#include <array>
#include <format>
#include <limits>

#include "../core/Audio.h"

#include "../core/vkcodes.h"
#include "remixicon.h"

void CalculateDirectionVectors(XMVECTOR& outForward, XMVECTOR& outRight, XMVECTOR inRotation)
{
	XMMATRIX camRotation = XMMatrixRotationQuaternion(inRotation);
	XMVECTOR right{ 1, 0, 0 };
	XMVECTOR forward{ 0, 0, 1 };
	outForward = XMVector3Transform(forward, camRotation);
	outRight = XMVector3Transform(right, camRotation);
}

XMVECTOR SampleAnimation(AnimationData& animData, float animationTime, XMVECTOR(__vectorcall* interp)(XMVECTOR a, XMVECTOR b, float t))
{
	assert(animData.frameCount > 0);
	for (int i = 0; i < animData.frameCount; i++)
	{
		if (animData.times[i] > animationTime)
		{
			if (i == 0)
			{
				return animData.data[0];
			}
			else
			{
				float t = (animationTime - animData.times[i - 1]) / (animData.times[i] - animData.times[i - 1]);
				return interp(animData.data[i - 1], animData.data[i], t);
			}
			break;
		}
	}
	return animData.data[animData.frameCount - 1];
}

CollisionResult Game::CollideWithWorld(const XMVECTOR rayOrigin, const XMVECTOR rayDirection, uint64_t matchingLayers)
{
	CollisionResult result{ nullptr, std::numeric_limits<float>::max() };

	for (Entity& entity : entityArena)
	{
		if (entity.isActive && (entity.collisionLayers & matchingLayers) != 0)
		{
			XMVECTOR entityCenterWorld = entity.position + entity.LocalToWorld(entity.aabbLocalPosition);
			XMVECTOR entitySizeWorld = entity.LocalToWorld(entity.aabbLocalSize);

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
				result.entity = &entity;
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
	ShaderDescription laserShader{ L"laser.hlsl", "VSMain", "PSMain", L"Laser" };

	// Textures
	diffuseTexture = engine.CreateTexture(L"textures/ground_D.dds");
	memeTexture = engine.CreateTexture(L"textures/cat_D.dds");

	// Materials
	size_t memeMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { memeTexture }, defaultShader);
	size_t groundMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, groundShader);
	size_t laserMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), {}, laserShader);

	// Meshes
	// TODO: why does mesh need material index, and why doesn't it matter if it's wrong?
	MeshFile cubeMeshFile = LoadGltfFromFile("models/cube.glb", debugLog, modelArena).meshes[0];
	auto cubeMeshView = engine.CreateMesh(memeMaterialIndex, cubeMeshFile.vertices, cubeMeshFile.vertexCount);
	engine.cubeVertexView = cubeMeshView;

	// Entities
	playerEntity = CreateEmptyEntity(engine);
	playerEntity->name = "Player";
	cameraEntity = CreateEmptyEntity(engine);
	cameraEntity->name = "Camera";
	cameraEntity->position = { 0.f, 2.f, 0.f };
	playerEntity->AddChild(cameraEntity);

	Entity* kaijuMeshEntity = CreateEntityFromGltf(engine, "models/kaiju.glb", defaultShader, debugLog);
	assert(kaijuMeshEntity->isSkinnedRoot);
	kaijuMeshEntity->name = "KaijuRoot";
	kaijuMeshEntity->transformHierachy->animationActive = true;
	kaijuMeshEntity->transformHierachy->animationLoop = true;
	kaijuMeshEntity->transformHierachy->animationIndex = kaijuMeshEntity->transformHierachy->animationNameToIndex.at("GameIdle");
	
	playerEntity->AddChild(kaijuMeshEntity);

	for (int i = 0; i < kaijuMeshEntity->childCount; i++)
	{
		// TODO: auto calculate bounds on mesh creation
		Entity* entity = kaijuMeshEntity->children[i];
		entity->aabbLocalPosition = { 0.f, 1.5f, -0.5f };
		entity->aabbLocalSize = { 5.f, 3.f, 3.f };
	}

	lightDebugEntity = CreateMeshEntity(engine, memeMaterialIndex, cubeMeshView);
	lightDebugEntity->name = "LightDebug";
	lightDebugEntity->scale = { .1f, .1f, .1f };

	for (int i = 0; i < MAX_ENENMY_COUNT; i++)
	{
		Entity* enemy = enemies[i] = CreateMeshEntity(engine, memeMaterialIndex, cubeMeshView);
		enemy->name = "Enemy";
		enemy->Disable();
		enemy->isEnemy = true;
		enemy->collisionLayers |= Dead;
	}

	for (int i = 0; i < MAX_PROJECTILE_COUNT; i++)
	{
		Entity* projectile = projectiles[i] = CreateMeshEntity(engine, laserMaterialIndex, cubeMeshView);
		projectile->name = "Projectile";
		projectile->Disable();
		projectile->isProjectile = true;
		projectile->scale = { .1f, .1f, .1f };
	}

	laser = CreateMeshEntity(engine, laserMaterialIndex, cubeMeshView);
	laser->name = "Laser";
	laser->checkForShadowBounds = false;
	laser->Disable();
	playerEntity->AddChild(laser);

	Entity* groundEntity = CreateQuadEntity(engine, groundMaterialIndex, 100.f, 100.f);
	groundEntity->name = "Ground";
	groundEntity->GetBuffer().color = { 1.f, 1.f, 1.f };
	groundEntity->collisionLayers |= Floor;

	// Defaults
	camera.position = { 0.f, 10.f, 0.f };
	playerPitch = XM_PI;

	light.position = { 10.f, 10.f, 10.f };

	// Audio
	soundFiles[(size_t)AudioFile::PlayerDamage] = LoadAudioFile(L"audio/chord.wav", globalArena);
	soundFiles[(size_t)AudioFile::EnemyDeath] = LoadAudioFile(L"audio/tada.wav", globalArena);
	soundFiles[(size_t)AudioFile::Shoot] = LoadAudioFile(L"audio/laser.wav", globalArena);
	
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
	engine.m_debugLineData.lineVertices.clear();

	// Debug Controls
	if (input.KeyJustPressed(VK_ESCAPE))
	{
		showEscMenu = !showEscMenu;
		UpdateCursorState();
	}
	if (input.KeyDown(VK_LBUTTON))
	{
		CollisionResult collision = CollideWithWorld(camera.position, camForward, ClickTest);
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
		const float maxPitch = XM_PI * 0.49;

		playerYaw += input.mouseDeltaX * 0.003f;
		playerPitch += input.mouseDeltaY * 0.003f;
		if (playerPitch > maxPitch) playerPitch = maxPitch;
		if (playerPitch < -maxPitch) playerPitch = -maxPitch;
	}

	playerEntity->rotation = XMQuaternionRotationRollPitchYaw(0.f, playerYaw, 0.f);
	cameraEntity->rotation = XMQuaternionRotationRollPitchYaw(playerPitch, 0.f, 0.f);

	// Player movement
	if (input.KeyComboJustPressed(VK_KEY_R, VK_CONTROL))
	{
		playerEntity->position = { 0.f, 0.f, 0.f };
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
		const float collisionEpsilon = 0.1;
		CollisionResult floorCollision = CollideWithWorld(playerEntity->position + XMVECTOR{ 0., collisionEpsilon, 0. }, V3_DOWN, Floor);
		bool onGround = floorCollision.distance <= collisionEpsilon;
		if (onGround)
		{
			// Move player out of ground
			playerEntity->position = playerEntity->position + XMVectorScale(V3_DOWN, floorCollision.distance);

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
		playerEntity->position += playerVelocity * engine.m_updateDeltaTime;
	}

	// Spawn enemies
	if (spawnEnemies && engine.TimeSinceStart() >= lastEnemySpawn + enemySpawnRate)
	{
		lastEnemySpawn = engine.TimeSinceStart();

		bool enemySpawned = false;
		for (int i = 0; i < MAX_ENENMY_COUNT; i++)
		{
			Entity* enemy = enemies[i];
			if (!enemy->isActive)
			{
				enemy->isActive = true;
				enemy->GetData().visible = true;
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
		Warn("DED");
		PlaySound(engine, &playerAudioSource, AudioFile::PlayerDamage);
		enemyCollision.entity->Disable();
	}

	// Update Projectiles
	for (int i = 0; i < MAX_PROJECTILE_COUNT; i++)
	{
		Entity* projectile = projectiles[i];
		if (!projectile->isActive) continue;

		if (engine.TimeSinceStart() > projectile->spawnTime + projectileLifetime)
		{
			projectile->Disable();
			continue;
		}

		projectile->position += projectile->velocity * engine.m_updateDeltaTime;

		CollisionResult projectileFloorCollision = CollideWithWorld(projectile->position, V3_DOWN, Floor);
		if (projectileFloorCollision.distance <= 0.f)
		{
			projectile->velocity.m128_f32[1] *= -1.f;
			projectile->position = projectile->position + V3_DOWN * projectileFloorCollision.distance;
		}

		CollisionResult projectileEnemyCollision = CollideWithWorld(projectile->position, V3_DOWN, Dead);
		if (projectileEnemyCollision.distance <= 0.f)
		{
			projectile->Disable();
			projectileEnemyCollision.entity->Disable();
			PlaySound(engine, &projectileEnemyCollision.entity->audioSource, AudioFile::EnemyDeath);
		}
	}

	if (engine.TimeSinceStart() > laser->spawnTime + laserLifetime)
	{
		laser->Disable();
	}

	// Shoot!
	/*if (input.KeyDown(VK_LBUTTON) && engine.TimeSinceStart() > lastProjectileSpawn + projectileSpawnRate)
	{
		for (int i = 0; i < MAX_PROJECTILE_COUNT; i++)
		{
			Entity* projectile = projectiles[i];
			if (projectile->isActive) continue;

			projectile->isActive = true;
			projectile->spawnTime = engine.TimeSinceStart();
			projectile->position = camera.position + PLAYER_HAND_OFFSET;
			projectile->rotation = camera.rotation;
			projectile->velocity = camForward * projectileSpeed + playerVelocity;
			projectile->GetData().visible = true;
			PlaySound(engine, &projectile->audioSource, AudioFile::Shoot);

			lastProjectileSpawn = engine.TimeSinceStart();
			break;
		}
	}*/

	// Laser
	if (input.KeyDown(VK_LBUTTON) && engine.TimeSinceStart() > lastLaserSpawn + laserSpawnRate)
	{
		laser->isActive = true;
		laser->spawnTime = engine.TimeSinceStart();
		laser->GetData().visible = true;

		XMVECTOR handPosition = camera.position + XMVector3Transform(PLAYER_HAND_OFFSET, engine.m_sceneConstantBuffer.data.cameraView);
		laser->position = handPosition + camForward * LASER_LENGTH / 2.f;
		laser->rotation = camera.rotation;
		laser->scale = { .1f, .1f, LASER_LENGTH };
		
		lastLaserSpawn = engine.TimeSinceStart();
		PlaySound(engine, &playerAudioSource, AudioFile::Shoot);

		bool keepHitting = true;
		while (keepHitting)
		{
			CollisionResult collision = CollideWithWorld(handPosition, camForward, Dead);
			keepHitting = collision.distance <= LASER_LENGTH;
			if (keepHitting)
			{
				collision.entity->Disable();
				PlaySound(engine, &collision.entity->audioSource, AudioFile::EnemyDeath);
			}
		}
	}

	// Player Audio
	XMStoreFloat3(&playerAudioListener.OrientFront, camForward);
	XMStoreFloat3(&playerAudioListener.OrientTop, XMVector3Cross(camForward, camRight));
	XMStoreFloat3(&playerAudioListener.Position, camera.position);
	XMStoreFloat3(&playerAudioListener.Velocity, playerVelocity);

	// Update entities
	for (Entity& entity : entityArena)
	{
		// Update animation transforms
		if (entity.isSkinnedRoot)
		{
			TransformHierachy& hierachy = *entity.transformHierachy;

			if (hierachy.animationActive)
			{
				hierachy.animationTime = fmodf(engine.TimeSinceStart(), hierachy.animations[hierachy.animationIndex].duration);
			}

			for (int jointIdx = 0; jointIdx < hierachy.nodeCount; jointIdx++)
			{
				TransformNode& node = hierachy.nodes[jointIdx];
				if (!hierachy.animationActive)
				{
					node.currentLocal = node.baseLocal;
				}
				else
				{
					XMVECTOR scale;
					XMVECTOR rotation;
					XMVECTOR translation;
					XMMatrixDecompose(&scale, &rotation, &translation, node.baseLocal);

					TransformAnimation& animation = hierachy.animations[hierachy.animationIndex];
					AnimationData& translationData = animation.jointChannels[jointIdx].translations;
					AnimationData& rotationData = animation.jointChannels[jointIdx].rotations;
					AnimationData& scaleData = animation.jointChannels[jointIdx].scales;

					if (translationData.frameCount > 0)
					{
						translation = SampleAnimation(translationData, hierachy.animationTime, &XMVectorLerp);
					}
					if (rotationData.frameCount > 0)
					{
						rotation = SampleAnimation(rotationData, hierachy.animationTime, &XMQuaternionSlerp);
					}
					if (scaleData.frameCount > 0)
					{
						scale = SampleAnimation(scaleData, hierachy.animationTime, &XMVectorLerp);
					}

					node.currentLocal = XMMatrixAffineTransformation(scale, {}, rotation, translation);
				}
				hierachy.UpdateNode(&node);
			}

			// Upload new transforms to children
			for (int childIdx = 0; childIdx < entity.childCount; childIdx++)
			{
				Entity& child = *entity.children[childIdx];
				if (child.isSkinnedMesh && child.isRendered)
				{
					EntityData& data = child.GetData();
					for (int i = 0; i < hierachy.nodeCount; i++)
					{
						assert(i < MAX_BONES);
						data.boneConstantBuffer.data.inverseJointBinds[i] = XMMatrixTranspose(hierachy.nodes[i].inverseBind);
						data.boneConstantBuffer.data.jointTransforms[i] = XMMatrixTranspose(hierachy.nodes[i].global);
					}
				}
			}
		}

		if (entity.isRendered) {
			entity.GetBuffer().aabbLocalPosition = entity.aabbLocalPosition;
			entity.GetBuffer().aabbLocalSize = entity.aabbLocalSize;
		}

		XMVECTOR entityForwards;
		XMVECTOR entityRight;
		CalculateDirectionVectors(entityForwards, entityRight, entity.rotation);
		XMVECTOR entityUp = XMVector3Cross(entityForwards, entityRight);

		// Update entity audio
		IXAudio2SourceVoice* audioSourceVoice = entity.audioSource.source;
		if (audioSourceVoice != nullptr)
		{
			X3DAUDIO_EMITTER& emitter = entity.audioSource.audioEmitter;
			XMStoreFloat3(&emitter.OrientFront, entityForwards);
			XMStoreFloat3(&emitter.OrientTop, entityUp);
			XMStoreFloat3(&emitter.Position, entity.position);
			XMStoreFloat3(&emitter.Velocity, entity.velocity);

			XAUDIO2_VOICE_DETAILS audioSourceDetails;
			audioSourceVoice->GetVoiceDetails(&audioSourceDetails);

			X3DAUDIO_DSP_SETTINGS* dspSettings = emitter.ChannelCount == 1 ? &engine.m_audioDspSettingsMono : &engine.m_audioDspSettingsStereo;

			X3DAudioCalculate(engine.m_3daudio, &playerAudioListener, &entity.audioSource.audioEmitter, X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_REVERB, dspSettings);
			audioSourceVoice->SetOutputMatrix(engine.m_audioMasteringVoice, audioSourceDetails.InputChannels, engine.m_audioVoiceDetails.InputChannels, dspSettings->pMatrixCoefficients);
			audioSourceVoice->SetFrequencyRatio(dspSettings->DopplerFactor);
			// TODO: low pass filter + reverb
		}
	}

	// Update entity matrices
	for (Entity& entity : entityArena)
	{
		if (entity.parent == nullptr)
		{
			entity.UpdateWorldMatrix();
		}
	}


	// Update Camera
	XMVECTOR camTranslation;
	XMVECTOR camRotation;
	XMVECTOR camScale; //ignored
	XMMatrixDecompose(&camScale, &camRotation, &camTranslation, cameraEntity->worldMatrix);
	camera.position = camTranslation;
	camera.rotation = camRotation;
	CalculateDirectionVectors(camForward, camRight, camera.rotation);

	engine.m_sceneConstantBuffer.data.cameraView = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(camera.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(camera.rotation)));
	engine.m_sceneConstantBuffer.data.cameraProjection = XMMatrixTranspose(XMMatrixPerspectiveFovLH(camera.fovY, engine.m_aspectRatio, camera.nearClip, camera.farClip));
	engine.m_sceneConstantBuffer.data.postProcessing = { contrast, brightness, saturation, fog };
	engine.m_sceneConstantBuffer.data.fogColor = clearColor;
	engine.m_sceneConstantBuffer.data.worldCameraPos = camera.position;

	// Update light/shadowmap
	light.rotation = XMQuaternionRotationRollPitchYaw(45.f / 360.f * XM_2PI, engine.TimeSinceStart(), 0.f);
	engine.m_lightConstantBuffer.data.lightView = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(light.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(light.rotation)));

	lightDebugEntity->position = light.position;
	lightDebugEntity->rotation = light.rotation;

	if (showLightPosition)
	{
		engine.m_debugLineData.AddLine(light.position, light.position + XMVector3TransformNormal({ 0.f, 0.f, 1.f }, XMMatrixRotationQuaternion(light.rotation)), { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });
	}

	XMVECTOR lightSpaceMin{};
	XMVECTOR lightSpaceMax{};
	bool firstLightCalculation = true;

	const XMVECTOR testPositionBuffer[8] = {
		{ -1., -1., -1.},
		{ -1., -1.,  1.},
		{ -1.,  1., -1.},
		{ -1.,  1.,  1.},
		{  1., -1., -1.},
		{  1., -1.,  1.},
		{  1.,  1., -1.},
		{  1.,  1.,  1.},
	};
	for (Entity& entity : entityArena)
	{
		if (!entity.checkForShadowBounds || !entity.isRendered || !entity.GetData().visible) continue;

		XMVECTOR aabbWorldPosition = XMVector3Transform(entity.aabbLocalPosition, entity.worldMatrix);
		XMVECTOR aabbWorldSize = XMVector3TransformNormal(entity.aabbLocalSize, entity.worldMatrix);

		for (int i = 0; i < _countof(testPositionBuffer); i++)
		{
			XMVECTOR aabbCorner = XMVectorMultiply(aabbWorldSize, testPositionBuffer[i]);
			XMVECTOR lightSpacePosition = XMVector3Transform(aabbWorldPosition + aabbCorner, XMMatrixTranspose(engine.m_lightConstantBuffer.data.lightView));
			lightSpaceMin = firstLightCalculation ? lightSpacePosition : XMVectorMin(lightSpacePosition, lightSpaceMin);
			lightSpaceMax = firstLightCalculation ? lightSpacePosition : XMVectorMax(lightSpacePosition, lightSpaceMax);
			firstLightCalculation = false;
		}
	}

	XMVECTOR lightRight;
	CalculateDirectionVectors(engine.m_lightConstantBuffer.data.sunDirection, lightRight, light.rotation);
	XMFLOAT3 lsMin;
	XMFLOAT3 lsMax;
	XMStoreFloat3(&lsMin, lightSpaceMin);
	XMStoreFloat3(&lsMax, lightSpaceMax);

	engine.m_lightConstantBuffer.data.lightProjection = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(lsMin.x, lsMax.x, lsMin.y, lsMax.y, lsMin.z, lsMax.z));
	
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
			*engine.m_debugLineData.lineVertices.new_element() = { edgeStart3, cubeColor };
			*engine.m_debugLineData.lineVertices.new_element() = { edgeEnd3, cubeColor };
		}
	}

	// Post Processing
	XMVECTOR camPos2d = camera.position;
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

Entity* Game::CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height)
{
	MeshFile file = CreateQuad(width, height, modelArena);
	auto meshView = engine.CreateMesh(materialIndex, file.vertices, file.vertexCount);
	Entity* entity = CreateMeshEntity(engine, materialIndex, meshView);
	entity->position = { -width / 2.f, 0.f, -width / 2.f };
	entity->aabbLocalPosition = { width / 2.f, -.05f, width / 2.f };
	entity->aabbLocalSize = { width, .1f, width };
	return entity;
}


Entity* Game::CreateEntityFromGltf(EngineCore& engine, const char* path, ShaderDescription& shader, RingLog& log)
{
	Entity* mainEntity = CreateEmptyEntity(engine);

	GltfResult gltfResult = LoadGltfFromFile(path, log, modelArena);
	if (gltfResult.transformHierachy != nullptr)
	{
		mainEntity->isSkinnedRoot = true;
		mainEntity->transformHierachy = gltfResult.transformHierachy;
	}

	for (MeshFile& meshFile : gltfResult.meshes)
	{
		std::wstring texturePath = { L"textures/" };
		texturePath.append(meshFile.textureName.begin(), meshFile.textureName.end());
		Texture* texture = engine.CreateTexture(texturePath.c_str());

		size_t materialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { texture }, shader);
		D3D12_VERTEX_BUFFER_VIEW meshView = engine.CreateMesh(materialIndex, meshFile.vertices, meshFile.vertexCount);

		Entity* child = CreateMeshEntity(engine, materialIndex, meshView);
		child->name = meshFile.textureName.c_str();
		if (gltfResult.transformHierachy != nullptr) child->isSkinnedMesh = true;
		mainEntity->AddChild(child);
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
