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

XMVECTOR GetPiecePosition(Entity* entity, const PuzzlePiece& piece)
{
	XMFLOAT3 scale;
	XMStoreFloat3(&scale, entity->scale);
	return { (float)piece.x * (1.f + BLOCK_DISPLAY_GAP) + scale.x / 2.f - 0.5f, 0.f, (float)piece.y * (1.f + BLOCK_DISPLAY_GAP) + scale.z / 2.f - 0.5f };
}

struct CollisionResult
{
	Entity* entity;
	float distance;
};

CollisionResult CollideWithWorld(Game& game, const XMVECTOR rayOrigin, const XMVECTOR rayDirection, uint64_t matchingLayers)
{
	CollisionResult result{ nullptr, std::numeric_limits<float>::max() };

	for (Entity* entity = (Entity*)game.entityArena.base; entity != (Entity*)(game.entityArena.base + game.entityArena.used); entity++)
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

Game::Game()
{
	displayedPuzzle = {};
	displayedPuzzle.width = 3;
	displayedPuzzle.height = 3;
	displayedPuzzle.pieceCount = 2;

	PuzzlePiece& p = displayedPuzzle.pieces[0];
	p.width = 2;
	p.height = 2;
	p.startX = 0;
	p.startY = 0;
	p.targetX = 1;
	p.targetY = 1;
	p.canMoveHorizontal = true;
	p.canMoveVertical = true;
	p.hasTarget = true;
	p.typeHash = 0;
	p.x = p.startX;
	p.y = p.startY;

	PuzzlePiece& p2 = displayedPuzzle.pieces[1];
	p2.width = 1;
	p2.height = 1;
	p2.startX = 2;
	p2.startY = 2;
	p2.targetX = 0;
	p2.targetY = 0;
	p2.canMoveHorizontal = true;
	p2.canMoveVertical = true;
	p2.hasTarget = true;
	p2.typeHash = 0;
	p2.x = p2.startX;
	p2.y = p2.startY;

	solver = NewObject(globalArena, PuzzleSolver, displayedPuzzle, debugLog);
}

void Game::StartGame(EngineCore& engine)
{
	// Shaders
	ShaderDescription defaultShader{ L"entity.hlsl", "VSMain", "PSMain", L"Main" };

	// Textures
	engine.CreateTexture(diffuseTexture, L"textures/ground-diffuse-bc1.dds", L"Diffuse Texture");
	engine.CreateTexture(memeTexture, L"textures/cat.dds", L"yea");
	engine.CreateTexture(kaijuTexture, L"textures/kaiju.dds", L"kaiju");

	// Materials
	size_t dirtMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &diffuseTexture }, defaultShader);
	size_t memeMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &memeTexture }, defaultShader);
	size_t kaijuMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), { &kaijuTexture }, defaultShader);

	// Meshes
	MeshFile cubeMeshFile = LoadMeshFromFile("models/cube.obj", "models/", debugLog, vertexUploadArena, indexUploadArena);
	auto cubeMeshView = engine.CreateMesh(dirtMaterialIndex, cubeMeshFile.vertices, cubeMeshFile.vertexCount);
	engine.cubeVertexView = cubeMeshView;

	MeshFile kaijuMeshFile = LoadMeshFromFile("models/kaiju.obj", "models/", debugLog, vertexUploadArena, indexUploadArena);
	auto kaijuMeshView = engine.CreateMesh(memeMaterialIndex, kaijuMeshFile.vertices, kaijuMeshFile.vertexCount);

	// Entities
	Entity* kaijuEntity = CreateEntity(engine, kaijuMaterialIndex, kaijuMeshView);
	kaijuEntity->GetBuffer()->color = { 1.f, 1.f, 1.f };
	kaijuEntity->GetData()->aabbLocalPosition = { 0.f, 5.f, 0.f };
	kaijuEntity->GetData()->aabbLocalSize = { 4.f, 10.f, 2.f };

	for (int i = 0; i < displayedPuzzle.pieceCount; i++)
	{
		PuzzlePiece& piece = displayedPuzzle.pieces[i];
		Entity* pieceEntity = puzzleEntities[i] = CreateEntity(engine, memeMaterialIndex, cubeMeshView);
		pieceEntity->GetBuffer()->color = {.5f, .5f, .5f};
		pieceEntity->scale = { (float)piece.width + BLOCK_DISPLAY_GAP * (piece.width - 1), 1.f, (float)piece.height + BLOCK_DISPLAY_GAP * (piece.height - 1)};
		pieceEntity->collisionLayers = ClickTest;
		pieceEntity->position = GetPiecePosition(puzzleEntities[i], piece);
	}

	float boardWidth = displayedPuzzle.width + BLOCK_DISPLAY_GAP * (displayedPuzzle.width - 1);
	float boardHeight = displayedPuzzle.height + BLOCK_DISPLAY_GAP * (displayedPuzzle.height - 1);
	MeshFile quadFile = CreateQuad(boardWidth, boardHeight, vertexUploadArena, indexUploadArena);
	auto quadMeshView = engine.CreateMesh(dirtMaterialIndex, quadFile.vertices, quadFile.vertexCount);
	Entity* quadEntity = CreateEntity(engine, dirtMaterialIndex, quadMeshView);
	quadEntity->position = { -0.5f, -0.5f, -0.5f };
	quadEntity->GetBuffer()->color = {0.f, .5f, 0.f};

	MeshFile groundFile = CreateQuad(100, 100, vertexUploadArena, indexUploadArena);
	auto groundMeshView = engine.CreateMesh(dirtMaterialIndex, groundFile.vertices, groundFile.vertexCount);
	Entity* groundEntity = CreateEntity(engine, dirtMaterialIndex, groundMeshView);
	groundEntity->position = { -50.f, -0.51f, -50.f };
	groundEntity->GetBuffer()->color = { 1.f, 1.f, 1.f };

	camera.position = { 0.f, 10.f, 0.f };
	playerPitch = XM_PI;

	light.position = { 10.f, 10.f, 10.f };

	engine.UploadVertices();
	UpdateCursorState();
}

CoroutineReturn DisplayPuzzleSolution(std::coroutine_handle<>* handle, Game* game, EngineCore* engine)
{
	CoroutineAwaiter awaiter{ handle };

	for (int i = 0; i < game->solver->solvedPosition.path.moveCount; i++)
	{
		float moveStartTime = engine->TimeSinceStart();
		PuzzleMove& move = game->solver->solvedPosition.path.moves[i];
		PuzzlePiece& piece = game->displayedPuzzle.pieces[move.pieceIndex];

		XMVECTOR startPos = GetPiecePosition(game->puzzleEntities[move.pieceIndex], piece);
		piece.x += move.x;
		piece.y += move.y;
		XMVECTOR endPos = GetPiecePosition(game->puzzleEntities[move.pieceIndex], piece);
		
		float timeInStep = 0.f;
		do
		{
			co_await awaiter;
			timeInStep = engine->TimeSinceStart() - moveStartTime;
			if (timeInStep > SOLUTION_PLAYBACK_SPEED) timeInStep = SOLUTION_PLAYBACK_SPEED;

			float progress = timeInStep / SOLUTION_PLAYBACK_SPEED;
			float progressSq = progress * progress;
			float progressCb = progressSq * progress;
			float smoothProgress = 3.f * progressSq - 2.f * progressCb;
			game->puzzleEntities[move.pieceIndex]->position = XMVectorLerp(startPos, endPos, smoothProgress);

		} while (timeInStep < SOLUTION_PLAYBACK_SPEED);
	}
}

void CalculateDirectionVectors(XMVECTOR& outForward, XMVECTOR& outRight, XMVECTOR inRotation)
{
	XMMATRIX camRotation = XMMatrixRotationQuaternion(inRotation);
	XMVECTOR right{ 1, 0, 0 };
	XMVECTOR forward{ 0, 0, 1 };
	outForward = XMVector3Transform(forward, camRotation);
	outRight = XMVector3Transform(right, camRotation);
}

void Game::UpdateGame(EngineCore& engine)
{
	light.rotation = XMQuaternionRotationRollPitchYaw(45.f / 360.f * XM_2PI, 225.f / 360.f * XM_2PI, 0.f);

	XMVECTOR camForward;
	XMVECTOR camRight;
	CalculateDirectionVectors(camForward, camRight, camera.rotation);

	// Reset per frame values
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		entity->GetBuffer()->isSelected = { 0 };
	}

	// Read Inputs
	engine.BeginProfile("Input", ImColor::HSV(.5f, 1.f, .5f));
	input.accessMutex.lock();
	input.UpdateMousePosition();

	float camSpeed = 10.f;
	if (input.KeyDown(VK_SHIFT)) camSpeed *= .1f;
	if (input.KeyDown(VK_CONTROL)) camSpeed *= 5.f;

	if (input.KeyJustPressed(VK_ESCAPE))
	{
		showEscMenu = !showEscMenu;
		UpdateCursorState();
	}
	if (input.KeyComboJustPressed(VK_KEY_R, VK_CONTROL))
	{
		camera.position = { 0.f, 0.f, 0.f };
	}
	if (input.KeyDown(VK_KEY_A))
	{
		camera.position -= camRight * engine.m_updateDeltaTime * camSpeed;
	}
	if (input.KeyDown(VK_KEY_D))
	{
		camera.position += camRight * engine.m_updateDeltaTime * camSpeed;
	}
	if (input.KeyDown(VK_KEY_S))
	{
		camera.position -= camForward * engine.m_updateDeltaTime * camSpeed;
	}
	if (input.KeyDown(VK_KEY_W))
	{
		camera.position += camForward * engine.m_updateDeltaTime * camSpeed;
	}
	if (input.KeyDown(VK_LBUTTON))
	{
		CollisionResult collision = CollideWithWorld(*this, camera.position, camForward, ClickTest);
		if (collision.entity != nullptr)
		{
			collision.entity->GetBuffer()->isSelected = { 1 };
		}
	}
	if (input.KeyJustPressed(VK_F1))
	{
		puzzleArena.Reset();
		solver->Solve();
		if (solver->solved)
		{
			displayedPuzzle = solver->puzzle;
			for (int i = 0; i < displayedPuzzle.pieceCount; i++)
			{
				puzzleEntities[i]->position = GetPiecePosition(puzzleEntities[i], displayedPuzzle.pieces[i]);
			}
			DisplayPuzzleSolution(&displayCoroutine, this, &engine);
			//DisplayPuzzleSolution(puzzleArena, &displayCoroutine, this, &engine);

			/*int depthOffsets[1024]{0};
			for (int i = 0; i < solver->currentPendingIndex; i++)
			{
				SlidingPuzzle& puzzle = solver->pendingPositions[i];
				int depth = puzzle.path.moveCount;
				Entity* entity = graphDisplayEntities[i];
				entity->position = { (float)depth * 1.1f, 0.f, 3.5f + depthOffsets[depth]++ * 1.1f };
				entity->scale = { .5f, .5f, .5f };
				engine.m_entities[entity->dataIndex].visible = true;
				if (puzzle.distance == 0)
				{
					entity->GetBuffer(engine).color = { 207.f / 256.f, 159.f / 256.f, 27.f / 256.f };
				}
				else
				{
					entity->GetBuffer(engine).color = { .5f, .5f, .5f };
				}
			}*/
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
		const float maxPitch = XM_PI * 0.5;

		playerYaw += input.mouseDeltaX * 0.003f;
		playerPitch += input.mouseDeltaY * 0.003f;
		if (playerPitch > maxPitch) playerPitch = maxPitch;
		if (playerPitch < -maxPitch) playerPitch = -maxPitch;
	}

	input.NextFrame();
	input.accessMutex.unlock();
	engine.EndProfile("Input");
	
	// Update game
	engine.BeginProfile("Game Update", ImColor::HSV(.75f, 1.f, 1.f));
	if (displayCoroutine != nullptr)
	{
		if (displayCoroutine.done())
		{
			displayCoroutine.destroy();
			displayCoroutine = nullptr;
		}
		else
		{
			displayCoroutine();
		}
	}

	// Update Camera
	camera.rotation = XMQuaternionMultiply(XMQuaternionRotationRollPitchYaw(playerPitch, 0.f, 0.f), XMQuaternionRotationRollPitchYaw(0.f, playerYaw, 0.f));
	CalculateDirectionVectors(camForward, camRight, camera.rotation);

	engine.m_sceneConstantBuffer.data.cameraTransform = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(camera.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(camera.rotation)));
	engine.m_sceneConstantBuffer.data.perspectiveTransform = XMMatrixTranspose(XMMatrixPerspectiveFovLH(camera.fovY, engine.m_aspectRatio, camera.nearClip, camera.farClip));
	engine.m_sceneConstantBuffer.data.worldCameraPos = camera.position;

	// Update light/shadowmap
	engine.m_lightConstantBuffer.data.lightTransform = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(light.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(light.rotation)));

	XMVECTOR lightSpaceMin{};
	XMVECTOR lightSpaceMax{};
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		XMVECTOR lightSpacePosition = XMVector3Transform(entity->position, engine.m_lightConstantBuffer.data.lightTransform);
		lightSpaceMin = XMVectorMin(lightSpacePosition, lightSpaceMin);
		lightSpaceMax = XMVectorMax(lightSpacePosition, lightSpaceMax);
	}

	XMVECTOR lightForward;
	XMVECTOR lightRight;
	CalculateDirectionVectors(lightForward, lightRight, light.rotation);
	XMStoreFloat3(&engine.m_lightConstantBuffer.data.sunDirection, lightForward);
	XMFLOAT3 lsMin;
	XMFLOAT3 lsMax;
	XMStoreFloat3(&lsMin, lightSpaceMin);
	XMStoreFloat3(&lsMax, lightSpaceMax);

	engine.m_lightConstantBuffer.data.lightPerspective = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(lsMin.x, lsMax.x, -10., 20., 1., 30.));

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

	engine.EndProfile("Game Update");

	// Draw UI
	if (!pauseProfiler)
	{
		lastDebugFrameIndex = (lastDebugFrameIndex + 1) % _countof(lastFrames);
		lastFrames[lastDebugFrameIndex].duration = static_cast<float>(engine.m_updateDeltaTime);
	}

	DrawUI(engine);
}

EngineInput& Game::GetInput()
{
	return input;
}

Entity* Game::CreateEntity(EngineCore& engine, size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshView)
{
	Entity* entity = NewObject(entityArena, Entity);
	entity->engine = &engine;
	entity->materialIndex = materialIndex;
	entity->dataIndex = engine.CreateEntity(materialIndex, meshView);
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

EntityData* Entity::GetData()
{
	return &engine->m_materials[materialIndex].entities[dataIndex];
}

EntityConstantBuffer* Entity::GetBuffer()
{
	return &GetData()->constantBuffer.data;
}

XMVECTOR Entity::LocalToWorld(XMVECTOR localPosition)
{
	return XMVector4Transform(localPosition, GetBuffer()->worldTransform);
}

XMVECTOR Entity::WorldToLocal(XMVECTOR worldPosition)
{
	XMVECTOR det;
	return XMVector4Transform(worldPosition, XMMatrixInverse(&det, GetBuffer()->worldTransform));
}