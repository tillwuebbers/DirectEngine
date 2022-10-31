#include "Puzzle.h"

#include <format>

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

	size_t currentPendingIndex = 0;
	while (currentPendingIndex < pendingPositionCount)
	{
		SlidingPuzzle& currentPosition = pendingPositions[currentPendingIndex];

		// Is this a solution?
		if (currentPosition.distance == 0)
		{
			solvedPosition = currentPosition;
			debugLog.Log(std::format("Solved puzzle in {} steps!", solvedPosition.path.moveCount));
			for (int i = 0; i < solvedPosition.path.moveCount; i++)
			{
				PuzzleMove& move = solvedPosition.path.moves[i];
				debugLog.Log(std::format("{} > ({},{})", move.pieceIndex, move.x, move.y));
			}
			solved = true;
			break;
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