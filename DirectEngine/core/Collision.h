#pragma once

#include "Memory.h"
#include "../Helpers.h"

#include <DirectXMath.h>
using namespace DirectX;

#include <vector>

enum class CollisionInitType
{
	None,
	CollisionBody,
	RigidBody,
};

#define CollisionLayersTable\
  EnumFlag(1,  GizmoClick, "Gizmo Click")\
  EnumFlag(2,  Floor,      "Floor")\
  EnumFlag(3,  Player,     "Player")

enum class CollisionLayers : uint32_t
{
	None = 0U,
#define EnumFlag(idx, name, str) name = 1U << (idx - 1),
	CollisionLayersTable
#undef EnumFlag
	All = ~0U
};

const std::vector<const char*> collisionLayerNames =
{
#define EnumFlag(idx, name, str) str,
	CollisionLayersTable
#undef EnumFlag
};;

inline CollisionLayers operator|(CollisionLayers a, CollisionLayers b)
{
	return static_cast<CollisionLayers>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline CollisionLayers operator&(CollisionLayers a, CollisionLayers b)
{
	return static_cast<CollisionLayers>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}
