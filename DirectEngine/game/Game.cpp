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
	entity->position = { (float)piece.x * 1.1f, 0.f, (float)piece.y * 1.1f };
}

Game::Game()
{
	displayedPuzzle = {};
	displayedPuzzle.width = 2;
	displayedPuzzle.height = 2;
	displayedPuzzle.pieceCount = 2;

	PuzzlePiece& p = displayedPuzzle.pieces[0];
	p.width = 1;
	p.height = 1;
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
	p2.startX = 1;
	p2.startY = 1;
	p2.targetX = 0;
	p2.targetY = 0;
	p2.canMoveHorizontal = true;
	p2.canMoveVertical = true;
	p2.hasTarget = false;
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
	for (int i = 0; i < solver->puzzle.pieceCount; i++)
	{
		PuzzlePiece& piece = solver->puzzle.pieces[i];
		puzzleEntities[i] = CreateEntity(engine, cubeMeshIndex);

		UpdatePiecePosition(puzzleEntities[i], piece);
	}

	MeshFile quadFile{};
	CreateQuad(quadFile, 3.f, 3.f);
	size_t quadMeshIndex = engine.CreateMesh(quadFile.vertices.data(), sizeof(Vertex), quadFile.vertices.size(), 1);
	Entity* quadEntity = CreateEntity(engine, quadMeshIndex);
	quadEntity->position = { 0.f, -0.5f, 0.f };

	camera.position = { 0.f, -2.f, 10.f };

	UpdateCursorState();
}

CoroutineReturn DisplayPuzzleSolution(MemoryArena& arena, std::coroutine_handle<>* handle, Game* game, EngineCore& engine)
{
	CoroutineAwaiter awaiter{ handle };

	float moveStartTime = engine.TimeSinceStart();

	for (int i = 0; i < game->solver->solvedPosition.path.moveCount; i++)
	{
		while (engine.TimeSinceStart() < moveStartTime + 1.f) co_await awaiter;
		moveStartTime += 1.f;

		PuzzleMove& move = game->solver->solvedPosition.path.moves[i];
		PuzzlePiece& piece = game->displayedPuzzle.pieces[move.pieceIndex];
		piece.x += move.x;
		piece.y += move.y;
		UpdatePiecePosition(game->puzzleEntities[move.pieceIndex], piece);
	}
}

void Game::UpdateGame(EngineCore& engine)
{
	XMMATRIX camRotation = XMMatrixTranspose(XMMatrixRotationQuaternion(camera.rotation));
	XMVECTOR right{ -1, 0, 0 };
	XMVECTOR forward{ 0, 0, -1 };
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
	if (input.KeyComboJustPressed(VK_KEY_S, VK_CONTROL))
	{
		puzzleArena.Reset();
		solver->Solve();
		if (solver->solved)
		{
			displayedPuzzle = solver->puzzle;
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
	camera.rotation = XMQuaternionMultiply(XMQuaternionRotationRollPitchYaw(0.f, -playerYaw, 0.f), XMQuaternionRotationRollPitchYaw(-playerPitch, 0.f, 0.f));

	engine.m_constantBufferData.cameraTransform = XMMatrixMultiplyTranspose(XMMatrixTranslationFromVector(camera.position), XMMatrixRotationQuaternion(camera.rotation));
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
