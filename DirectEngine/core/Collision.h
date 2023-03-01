#pragma once

#include <DirectXMath.h>
using namespace DirectX;

enum CollisionLayers : unsigned int
{
	None       = 0,
	GizmoClick = 1 << 1,
	Floor      = 1 << 2,
	Dead       = 1 << 3,
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

CollisionResult CollideWithWorld(CollisionData* data, size_t colliderCount, XMVECTOR rayOrigin, XMVECTOR rayDirection, CollisionLayers matchingLayers);

size_t CollideWithWorldList(CollisionData* data, size_t colliderCount, CollisionResult* resultArray, size_t maxResults, XMVECTOR rayOrigin, XMVECTOR rayDirection, CollisionLayers matchingLayers);