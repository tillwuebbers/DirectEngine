#include "Collision.h"

#include <assert.h>
#include <algorithm>
#include <limits>

bool CollisionCheck(CollisionData& collider, XMVECTOR rayOrigin, XMVECTOR rayDirection, CollisionResult& result, bool onlyWriteClosest)
{
	XMVECTOR boxMin = XMVector3Transform(collider.aabbLocalPosition - collider.aabbLocalSize / 2.f, collider.worldMatrix);
	XMVECTOR boxMax = XMVector3Transform(collider.aabbLocalPosition + collider.aabbLocalSize / 2.f, collider.worldMatrix);

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

	if (minDistance <= 0.f && maxDistance >= 0.f)
	{
		if (std::abs(minDistance) < std::abs(maxDistance))
		{
			result.distance = minDistance;
		}
		else
		{
			result.distance = maxDistance;
		}

		result.collider = &collider;
		return true;
	}

	if (minDistance >= 0.f && minDistance <= maxDistance)
	{
		if (!onlyWriteClosest || minDistance < result.distance)
		{
			result.distance = minDistance;
			result.collider = &collider;
			return true;
		}
	}
	
	return false;
}

CollisionResult CollideWithWorld(CollisionData* data, size_t colliderCount, XMVECTOR rayOrigin, XMVECTOR rayDirection, CollisionLayers matchingLayers)
{
	assert(data != nullptr);
	assert(XMVectorGetX(XMVector3Length(rayDirection)) > 0.99f);
	assert(XMVectorGetX(XMVector3Length(rayDirection)) < 1.01f);

	CollisionResult result{ nullptr, std::numeric_limits<float>::max() };

	for (int i = 0; i < colliderCount; i++)
	{
		CollisionData& collider = data[i];
		if (collider.isActive && (collider.collisionLayers & matchingLayers) != 0)
		{
			CollisionCheck(collider, rayOrigin, rayDirection, result, true);
		}
	}

	return result;
}

size_t CollideWithWorldList(CollisionData* data, size_t colliderCount, CollisionResult* resultArray, size_t maxResults, XMVECTOR rayOrigin, XMVECTOR rayDirection, CollisionLayers matchingLayers)
{
	if (maxResults <= 0) return 0;

	assert(data != nullptr);
	assert(resultArray != nullptr);
	assert(XMVectorGetX(XMVector3Length(rayDirection)) > 0.99f);
	assert(XMVectorGetX(XMVector3Length(rayDirection)) < 1.01f);

	int resultIndex = 0;

	for (int i = 0; i < colliderCount; i++)
	{
		CollisionData& collider = data[i];
		if (collider.isActive && (collider.collisionLayers & matchingLayers) != 0)
		{
			if (CollisionCheck(collider, rayOrigin, rayDirection, resultArray[resultIndex], false))
			{
				resultIndex++;
				if (resultIndex >= maxResults) return resultIndex;
			}
		}
	}

	return resultIndex;
}