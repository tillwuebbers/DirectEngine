#include "Game.h"

#include <array>
#include <format>

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

Entity* CollideWithWorld(const XMVECTOR rayOrigin, const XMVECTOR rayDirection, const Game& game)
{
	for (Entity* entity = (Entity*)game.entityArena.base; entity != (Entity*)(game.entityArena.base + game.entityArena.used); entity++)
	{
		if (entity->hasCubeCollision)
		{
			XMVECTOR boxMin = entity->position - entity->scale / 2.f;
			XMVECTOR boxMax = entity->position + entity->scale / 2.f;

			XMVECTOR t0 = XMVectorDivide(XMVectorSubtract(boxMin, rayOrigin), rayDirection);
			XMVECTOR t1 = XMVectorDivide(XMVectorSubtract(boxMax, rayOrigin), rayDirection);
			XMVECTOR tmin = XMVectorMin(t0, t1);
			XMVECTOR tmax = XMVectorMax(t0, t1);

			XMFLOAT3 tMinValues;
			XMFLOAT3 tMaxValues;
			XMStoreFloat3(&tMinValues, tmin);
			XMStoreFloat3(&tMaxValues, tmax);
			if (std::max(std::max(tMinValues.x, tMinValues.y), tMinValues.z) <= std::min(std::min(tMaxValues.x, tMaxValues.y), tMaxValues.z))
			{
				return entity;
			}
		}
	}

	return nullptr;
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
	engine.CreateTexture(diffuseTexture, L"textures/ground-diffuse-bc1.dds", L"Diffuse Texture");
	engine.CreateTexture(memeTexture, L"textures/cat.dds", L"yea");
	engine.CreateTexture(kaijuTexture, L"textures/kaiju.dds", L"kaiju");

	size_t dirtMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), &diffuseTexture);
	size_t memeMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), &memeTexture);
	size_t kaijuMaterialIndex = engine.CreateMaterial(1024 * 64, sizeof(Vertex), &kaijuTexture);

	MeshFile kaijuMeshFile = LoadMeshFromFile("models/kaiju.obj", "models/", debugLog, vertexUploadArena, indexUploadArena);
	auto kaijuMeshView = engine.CreateMesh(memeMaterialIndex, kaijuMeshFile.vertices, kaijuMeshFile.vertexCount);
	Entity* kaijuEntity = CreateEntity(engine, kaijuMaterialIndex, kaijuMeshView);
	kaijuEntity->GetBuffer()->color = { 1.f, 1.f, 1.f };

	MeshFile cubeMeshFile = LoadMeshFromFile("models/cube.obj", "models/", debugLog, vertexUploadArena, indexUploadArena);
	auto cubeMeshView = engine.CreateMesh(dirtMaterialIndex, cubeMeshFile.vertices, cubeMeshFile.vertexCount);

	testCube = CreateEntity(engine, dirtMaterialIndex, cubeMeshView);
	testCube->GetBuffer()->color = {1.f, 1.f, 1.f};
	testCube->scale = { .01f, .01f, .01f };

	/*MeshFile testQuadMesh{};
	CreateQuad(testQuadMesh, .2f, .2f);
	auto testQuadMeshView = engine.CreateMesh(dirtMaterialIndex, testQuadMesh.vertices.data(), testQuadMesh.vertices.size());
	Entity* testQuad = CreateEntity(engine, dirtMaterialIndex, testQuadMeshView);
	testQuad->GetBuffer(engine).isScreenSpace = true;*/

	/*for (int i = 0; i < _countof(graphDisplayEntities); i++)
	{
		Entity* graphEntity = graphDisplayEntities[i] = CreateEntity(engine, dirtMaterialIndex, cubeMeshView);
		engine.m_entities[graphEntity->dataIndex].visible = false;
	}*/

	for (int i = 0; i < displayedPuzzle.pieceCount; i++)
	{
		PuzzlePiece& piece = displayedPuzzle.pieces[i];
		Entity* pieceEntity = puzzleEntities[i] = CreateEntity(engine, memeMaterialIndex, cubeMeshView);
		pieceEntity->GetBuffer()->color = {.5f, .5f, .5f};
		pieceEntity->scale = { (float)piece.width + BLOCK_DISPLAY_GAP * (piece.width - 1), 1.f, (float)piece.height + BLOCK_DISPLAY_GAP * (piece.height - 1)};
		pieceEntity->hasCubeCollision = true;
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

	lightDisplayEntity = CreateEntity(engine, dirtMaterialIndex, cubeMeshView);
	lightDisplayEntity->scale = { .1f, .1f, .1f };

	camera.position = { 0.f, 10.f, 0.f };
	playerPitch = XM_PI;

	light.position = { 10.f, 10.f, 10.f };
	lightDisplayEntity->position = light.position;

	engine.UploadVertices();
	UpdateCursorState();
}

//CoroutineReturn DisplayPuzzleSolution(MemoryArena& arena, std::coroutine_handle<>* handle, Game* game, EngineCore* engine)
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
		camera.position -= camRight * engine.m_updateDeltaTime * 10.f;
	}
	if (input.KeyDown(VK_KEY_D))
	{
		camera.position += camRight * engine.m_updateDeltaTime * 10.f;
	}
	if (input.KeyDown(VK_KEY_S))
	{
		camera.position -= camForward * engine.m_updateDeltaTime * 10.f;
	}
	if (input.KeyDown(VK_KEY_W))
	{
		camera.position += camForward * engine.m_updateDeltaTime * 10.f;
	}
	if (input.KeyDown(VK_LBUTTON))
	{
		Entity* collision = CollideWithWorld(camera.position, camForward, *this);
		if (collision != nullptr)
		{
			collision->GetBuffer()->isSelected = { 1 };
		}
	}
	if (input.KeyComboJustPressed(VK_KEY_S, VK_CONTROL))
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

	// Update light
	XMVECTOR lightForward;
	XMVECTOR lightRight;
	CalculateDirectionVectors(lightForward, lightRight, light.rotation);
	XMStoreFloat3(&engine.m_lightConstantBuffer.data.sunDirection, lightForward);
	engine.m_lightConstantBuffer.data.lightTransform = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(light.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(light.rotation)));
	engine.m_lightConstantBuffer.data.lightPerspective = XMMatrixTranspose(XMMatrixOrthographicLH(10.f, 10.f, 1.f, 30.f));

	// Test
	testCube->position = XMVectorAdd(camera.position, camForward);

	// Update entities
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		if (entity->isSpinning)
		{
			entity->rotation = XMQuaternionRotationAxis(XMVECTOR{0.f, 1.f, 0.f}, static_cast<float>(engine.TimeSinceStart()));
		}

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

EntityConstantBuffer* Entity::GetBuffer()
{
	return &engine->m_materials[materialIndex].entities[dataIndex].constantBuffer.data;
}