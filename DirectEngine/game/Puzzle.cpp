#include "Puzzle.h"

#include <format>
#include <algorithm>
#include "../core/Coroutine.h"

/*void DisplayPuzzle()
{
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

			int depthOffsets[1024]{0};
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
			}
		}
	}
}*/

/*CoroutineReturn DisplayPuzzleSolution(std::coroutine_handle<>* handle, Game* game, EngineCore* engine)
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
}*/

/*XMVECTOR GetPiecePosition(Entity* entity, const PuzzlePiece& piece)
{
	XMFLOAT3 scale;
	XMStoreFloat3(&scale, entity->scale);
	return { (float)piece.x * (1.f + BLOCK_DISPLAY_GAP) + scale.x / 2.f - 0.5f, 0.f, (float)piece.y * (1.f + BLOCK_DISPLAY_GAP) + scale.z / 2.f - 0.5f };
}*/

void CreateTestPuzzle(SlidingPuzzle& displayedPuzzle)
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
}

void SetBitShape(uint64_t& bitboard, const PuzzlePiece& piece)
{
	for (int x = piece.x; x < piece.x + piece.width; x++)
	{
		for (int y = piece.y; y < piece.y + piece.height; y++)
		{
			bitboard |= 1ULL << (x + y * 8);
		}
	}
}

bool PieceCollides(const PuzzlePiece& piece, const uint64_t collisionBoard)
{
	uint64_t pieceBits = 0;
	SetBitShape(pieceBits, piece);
	return (collisionBoard & pieceBits) != 0ULL;
}

void SlidingPuzzle::CalculateHash()
{
	uint64_t finalHash = 13;

	for (int i = 0; i < pieceCount; i++)
	{
		uint64_t a = 63689;
		uint64_t b = 378551;

		PuzzlePiece& piece = pieces[i];

		uint64_t pieceHash = 61;
		pieceHash = pieceHash * a + piece.typeHash;
		a *= b;
		pieceHash = pieceHash * a + static_cast<uint64_t>(piece.x);
		a *= b;
		pieceHash = pieceHash * a + static_cast<uint64_t>(piece.y);

		finalHash *= pieceHash;
	}

	hash = finalHash;
}

void SlidingPuzzle::CalculateDistance()
{
	uint64_t newDistance = 0;

	for (int i = 0; i < pieceCount; i++)
	{
		PuzzlePiece& piece = pieces[i];
		if (piece.hasTarget)
		{
			newDistance += abs(piece.x - piece.targetX);
			newDistance += abs(piece.y - piece.targetY);
		}
	}

	distance = newDistance;
}

void SlidingPuzzle::CalculateBitboard()
{
	bitboard = 0;

	for (int i = 0; i < pieceCount; i++)
	{
		SetBitShape(bitboard, pieces[i]);
	}
}

PuzzleSolver::PuzzleSolver(SlidingPuzzle puzzle, RingLog& debugLog) : puzzle(puzzle), debugLog(debugLog)
{
	assert(puzzle.width <= MAX_PUZZLE_SIZE);
	assert(puzzle.height <= MAX_PUZZLE_SIZE);
	puzzle.CalculateHash();
	puzzle.CalculateDistance();
	pendingPositions[0] = puzzle;
	pendingPositionCount = 1;
	debugLog.Log("Initialized puzzle solver.");
}

void PuzzleSolver::Solve()
{
	debugLog.Log("Starting solve...");
	solved = false;

	currentPendingIndex = 0;
	while (currentPendingIndex < pendingPositionCount)
	{
		SlidingPuzzle& currentPosition = pendingPositions[currentPendingIndex];

		// Is this a solution?
		if (currentPosition.distance == 0)
		{
			if (!solved || currentPosition.path.moveCount < solvedPosition.path.moveCount)
			{
				solvedPosition = currentPosition;
				debugLog.Log(std::format("Solved puzzle in {} steps!", solvedPosition.path.moveCount));
				for (int i = 0; i < solvedPosition.path.moveCount; i++)
				{
					PuzzleMove& move = solvedPosition.path.moves[i];
					debugLog.Log(std::format("{} > ({},{})", move.pieceIndex, move.x, move.y));
				}
				solved = true;
			}
			else
			{
				debugLog.Log(std::format("Found worse solution with {} steps.", currentPosition.path.moveCount));
			}
		}

		// Check if position is already evaluated, abort if so
		bool known = false;
		for (int i = 0; i < knownPositionCount; i++)
		{
			if (knownPositionHashes[i] == currentPosition.hash)
			{
				known = true;
				break;
			}
		}

		if (known)
		{
			currentPendingIndex++;
			continue;
		}

		// Begin evaluating position
		knownPositionHashes[knownPositionCount] = currentPosition.hash;
		knownPositionCount++;

		currentPosition.CalculateBitboard();
		for (int i = 0; i < currentPosition.pieceCount; i++)
		{
			PuzzlePiece& piece = currentPosition.pieces[i];

			// Clear current piece from bitboard for collision detection
			uint64_t adjustedBitboard = currentPosition.bitboard;
			for (int x = piece.x; x < piece.x + piece.width; x++)
			{
				for (int y = piece.y; y < piece.y + piece.height; y++)
				{
					adjustedBitboard &= ~(1ULL << (x + y * 8));
				}
			}

			// Check if pieces can move, then add them to queue
			if (piece.canMoveHorizontal && piece.x + piece.width < puzzle.width)
			{
				piece.x += 1;
				if (!PieceCollides(piece, adjustedBitboard)) AddToQueue(&currentPosition, i, 1, 0);
				piece.x -= 1;
			}
			if (piece.canMoveHorizontal && piece.x > 0)
			{
				piece.x -= 1;
				if (!PieceCollides(piece, adjustedBitboard)) AddToQueue(&currentPosition, i, -1, 0);
				piece.x += 1;
			}
			if (piece.canMoveVertical && piece.y + piece.height < puzzle.height)
			{
				piece.y += 1;
				if (!PieceCollides(piece, adjustedBitboard)) AddToQueue(&currentPosition, i, 0, 1);
				piece.y -= 1;
			}
			if (piece.canMoveVertical && piece.y > 0)
			{
				piece.y -= 1;
				if (!PieceCollides(piece, adjustedBitboard)) AddToQueue(&currentPosition, i, 0, -1);
				piece.y += 1;
			}
		}

		debugLog.Log(std::format("{}: Distance={}, Bitboard={}", currentPosition.hash, currentPosition.distance, currentPosition.bitboard));
		currentPendingIndex++;
	}

	debugLog.Log("Finished solve...");

	std::sort(pendingPositions, pendingPositions + currentPendingIndex, [](const SlidingPuzzle& a, const SlidingPuzzle& b) {
		return a.path.moveCount < b.path.moveCount;
		});

	debugLog.Log("Finished sort.");

	for (int i = 0; i < currentPendingIndex; i++)
	{
		debugLog.Log(std::format("Depth: {}", pendingPositions[i].path.moveCount));
	}
}

void PuzzleSolver::AddToQueue(const SlidingPuzzle* currentPosition, size_t pieceIndex, int16_t x, int16_t y)
{
	assert(pendingPositionCount < MAX_PENDING_COUNT);
	SlidingPuzzle& newPosition = pendingPositions[pendingPositionCount];
	newPosition = *currentPosition;
	newPosition.CalculateHash();
	newPosition.CalculateDistance();
	PuzzleMove& move = newPosition.path.moves[newPosition.path.moveCount];
	move.pieceIndex = pieceIndex;
	move.x = x;
	move.y = y;
	newPosition.path.moveCount++;
	pendingPositionCount++;
}