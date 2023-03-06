#pragma once

#include <DirectXMath.h>
using namespace DirectX;

#include "Memory.h"

enum CollisionLayers : unsigned int
{
	None       = 0,
	GizmoClick = 1 << 1,
	Floor      = 1 << 2,
};

inline CollisionLayers operator|(CollisionLayers a, CollisionLayers b)
{
	return static_cast<CollisionLayers>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline CollisionLayers operator&(CollisionLayers a, CollisionLayers b)
{
	return static_cast<CollisionLayers>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

struct CollisionData
{
	XMMATRIX worldMatrix = XMMatrixIdentity();
	XMVECTOR aabbLocalPosition = { 0., 0., 0. };
	XMVECTOR aabbLocalSize = { 1., 1., 1. };
	
	bool isActive = true;
	unsigned int collisionLayers = 0;
	void* entity = nullptr;
	void* bone = nullptr;

	template <typename T>
	T* GetEntity()
	{
		return static_cast<T*>(entity);
	}
};

struct CollisionResult
{
	CollisionData* collider;
	float distance;
};

CollisionResult CollideWithWorld(FixedList<CollisionData>& colliders, XMVECTOR rayOrigin, XMVECTOR rayDirection, CollisionLayers matchingLayers);

void CollideWithWorldList(FixedList<CollisionData>& colliders, XMVECTOR rayOrigin, XMVECTOR rayDirection, CollisionLayers matchingLayers, FixedList<CollisionResult>& result);