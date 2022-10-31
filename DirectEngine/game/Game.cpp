#include "Game.h"

#include <array>
#include <format>

#include "../core/vkcodes.h"
#include "remixicon.h"

void CreateWorldMatrix(XMMATRIX& out, const XMVECTOR scale, const XMVECTOR rotation, const XMVECTOR position)
{
	out = DirectX::XMMatrixTranspose(DirectX::XMMatrixAffineTransformation(scale, XMVECTOR{}, rotation, position));
}

void UpdatePiecePosition(Entity* entity, const PuzzlePiece& piece)
{
	XMFLOAT3 scale;
	XMStoreFloat3(&scale, entity->scale);
	entity->position = { (float)piece.x * (1.f + BLOCK_DISPLAY_GAP) + scale.x / 2.f - 0.5f, 0.f, (float)piece.y * (1.f + BLOCK_DISPLAY_GAP) + scale.z / 2.f - 0.5f };
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
	/*size_t kaijuMeshIndex = LoadMeshFromFile(engine, "models/kaiju.obj", "models/");
	Entity* entity1 = CreateEntity(engine, kaijuMeshIndex);
	entity1->scale = XMVECTOR{ .1f, .1f, .1f };
	Entity* entity2 = CreateEntity(engine, kaijuMeshIndex);
	entity2->position = XMVECTOR{ 0.f, 0.f, 5.f };
	entity2->scale = XMVECTOR{ .3f, .3f, .3f };*/

	size_t cubeMeshIndex = CreateMeshFromFile(engine, "models/cube.obj", "models/");
	for (int i = 0; i < displayedPuzzle.pieceCount; i++)
	{
		PuzzlePiece& piece = displayedPuzzle.pieces[i];
		Entity* pieceEntity = puzzleEntities[i] = CreateEntity(engine, cubeMeshIndex);
		pieceEntity->color = { (float)(i % 10) / 10.f, (i / 10.f) / 10.f, 0.f};
		pieceEntity->scale = { (float)piece.width + BLOCK_DISPLAY_GAP * (piece.width - 1), 1.f, (float)piece.height + BLOCK_DISPLAY_GAP * (piece.height - 1)};
		pieceEntity->hasCubeCollision = true;

		UpdatePiecePosition(puzzleEntities[i], piece);
	}

	float floorWidth = displayedPuzzle.width + BLOCK_DISPLAY_GAP * (displayedPuzzle.width - 1);
	float floorHeight = displayedPuzzle.height + BLOCK_DISPLAY_GAP * (displayedPuzzle.height - 1);
	MeshFile quadFile{};
	CreateQuad(quadFile, floorWidth, floorHeight);
	size_t quadMeshIndex = engine.CreateMesh(quadFile.vertices.data(), sizeof(Vertex), quadFile.vertices.size(), 1);
	Entity* quadEntity = CreateEntity(engine, quadMeshIndex);
	quadEntity->position = { -0.5f, -0.5f, -0.5f };
	quadEntity->color = { 0.f, .5f, 0.f };

	testCube = CreateEntity(engine, cubeMeshIndex);
	testCube->color = { 1.f, 1.f, 1.f };
	testCube->scale = { .1f, .1f, .1f };

	camera.position = { 0.f, 10.f, 0.f };
	playerPitch = XM_PI;

	UpdateCursorState();
}

CoroutineReturn DisplayPuzzleSolution(MemoryArena& arena, std::coroutine_handle<>* handle, Game* game, EngineCore& engine)
{
	CoroutineAwaiter awaiter{ handle };

	float moveStartTime = engine.TimeSinceStart();

	for (int i = 0; i < game->solver->solvedPosition.path.moveCount; i++)
	{
		while (engine.TimeSinceStart() < moveStartTime + SOLUTION_PLAYBACK_SPEED) co_await awaiter;
		moveStartTime += SOLUTION_PLAYBACK_SPEED;

		PuzzleMove& move = game->solver->solvedPosition.path.moves[i];
		PuzzlePiece& piece = game->displayedPuzzle.pieces[move.pieceIndex];
		piece.x += move.x;
		piece.y += move.y;
		UpdatePiecePosition(game->puzzleEntities[move.pieceIndex], piece);
	}
}

void Game::UpdateGame(EngineCore& engine)
{
	XMMATRIX camRotation = XMMatrixRotationQuaternion(camera.rotation);
	XMVECTOR right{ 1, 0, 0 };
	XMVECTOR forward{ 0, 0, 1 };
	XMVECTOR camForward = XMVector3Transform(forward, camRotation);
	XMVECTOR camRight = XMVector3Transform(right, camRotation);

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
	if (input.KeyJustPressed(VK_LBUTTON))
	{
		testCube->position = XMVectorAdd(camera.position, camForward);

		Entity* collision = CollideWithWorld(camera.position, camForward, *this);
		if (collision != nullptr)
		{
			collision->color = { 1.f, 0.f, 0.f };
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
				UpdatePiecePosition(puzzleEntities[i], displayedPuzzle.pieces[i]);
			}
			DisplayPuzzleSolution(puzzleArena, &displayCoroutine, this, engine);
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

	// Update entities
	for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
	{
		EntityData& data = engine.m_entities[entity->dataIndex];

		if (entity->isSpinning)
		{
			entity->rotation = XMQuaternionRotationAxis(XMVECTOR{0.f, 1.f, 0.f}, static_cast<float>(engine.TimeSinceStart()));
		}

		CreateWorldMatrix(data.constantBufferData.worldTransform, entity->scale, entity->rotation, entity->position);
		data.constantBufferData.color = entity->color;

		// TODO: Race condition?
		memcpy(data.mappedConstantBufferData, &data.constantBufferData, sizeof(data.constantBufferData));
	}

	if (!showEscMenu)
	{
		const float maxPitch = XM_PI * 0.5;

		playerYaw += input.mouseDeltaX * 0.003f;
		playerPitch += input.mouseDeltaY * 0.003f;
		if (playerPitch > maxPitch) playerPitch = maxPitch;
		if (playerPitch < -maxPitch) playerPitch = -maxPitch;
	}
	camera.rotation = XMQuaternionMultiply(XMQuaternionRotationRollPitchYaw(playerPitch, 0.f, 0.f), XMQuaternionRotationRollPitchYaw(0.f, playerYaw, 0.f));

	engine.m_constantBufferData.cameraTransform = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(XMVectorScale(camera.position, -1.f)), XMMatrixRotationQuaternion(XMQuaternionInverse(camera.rotation)));
	engine.m_constantBufferData.clipTransform = XMMatrixTranspose(XMMatrixPerspectiveFovLH(camera.fovY, engine.m_aspectRatio, camera.nearClip, camera.farClip));

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

size_t Game::CreateMeshFromFile(EngineCore& engine, const std::string& filePath, const std::string& materialPath)
{
	MeshFile mesh{};
	LoadMeshFromFile(mesh, filePath, materialPath, debugLog);
	return engine.CreateMesh(mesh.vertices.data(), sizeof(Vertex), mesh.vertices.size(), 1);
}

Entity* Game::CreateEntity(EngineCore& engine, size_t meshIndex)
{
	Entity* entity = NewObject(entityArena, Entity);
	
	EntityData& data = engine.m_entities.emplace_back();
	entity->dataIndex = engine.m_entities.size() - 1;
	data.meshIndex = meshIndex;

	MeshData& mesh = engine.m_meshes[meshIndex];
	engine.CreateConstantBuffer<EntityConstantBuffer>(data.constantBuffer, &data.constantBufferData, &data.mappedConstantBufferData);
	data.constantBuffer->SetName(std::format(L"Entity Constant Buffer {}", meshIndex).c_str());
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
