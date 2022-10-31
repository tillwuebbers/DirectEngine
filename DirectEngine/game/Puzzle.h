#pragma once

#include "Log.h"
#include "../core/Memory.h"
#include <stdint.h>

const int MAX_PUZZLE_SIZE = 8;
const int MAX_PIECE_COUNT = 64;
const int MAX_PENDING_COUNT = 1024;
const int MAX_PATH_LENGTH = 1024;
const int MAX_KNOWN_POSITIONS = 1024;

struct PuzzlePiece
{
	int16_t width;
	int16_t height;
	int16_t startX;
	int16_t startY;
	int16_t targetX;
	int16_t targetY;
	bool canMoveHorizontal;
	bool canMoveVertical;
	bool hasTarget;
	uint64_t typeHash;

	int16_t x;
	int16_t y;
};

struct PuzzleMove
{
	size_t pieceIndex;
	int16_t x;
	int16_t y;
};

struct PuzzlePath
{
	PuzzleMove moves[MAX_PATH_LENGTH];
	size_t moveCount = 0;
};

struct SlidingPuzzle
{
	int16_t width = 0;
	int16_t height = 0;

	uint16_t pieceCount = 0;
	PuzzlePiece pieces[MAX_PIECE_COUNT];
	PuzzlePath path{};

	bool evaluated = false;
	uint64_t hash = 0;
	uint64_t distance = UINT64_MAX;
	uint64_t bitboard = 0;

	void CalculateHash();
	void CalculateDistance();
	void CalculateBitboard();
};

class PuzzleSolver
{
public:
	SlidingPuzzle puzzle;
	SlidingPuzzle solvedPosition;
	bool solved = false;

	RingLog& debugLog;
	MemoryArena solverArena{};
	SlidingPuzzle pendingPositions[MAX_PENDING_COUNT];
	size_t pendingPositionCount = 0;
	uint64_t knownPositionHashes[MAX_KNOWN_POSITIONS];
	size_t knownPositionCount = 0;

	PuzzleSolver(SlidingPuzzle puzzle, RingLog& debugLog);
	void Solve();
	void AddToQueue(const SlidingPuzzle* currentPosition, size_t pieceIndex, int16_t x, int16_t y);
};
