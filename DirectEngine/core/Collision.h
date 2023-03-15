#pragma once

#include "Memory.h"
#include "../Helpers.h"

#include <DirectXMath.h>
using namespace DirectX;

#include <reactphysics3d/reactphysics3d.h>

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

struct CollisionRecord
{
	XMVECTOR worldPoint = {};
	XMVECTOR worldNormal = {};
	reactphysics3d::decimal hitFraction = 0.f;
	reactphysics3d::decimal distance = 0.f;
	int meshSubpart = 0;
	int triangleIndex = 0;
	reactphysics3d::CollisionBody* body = nullptr;
	reactphysics3d::Collider* collider = nullptr;
};

void SetCollisionRecord(CollisionRecord& collision, const reactphysics3d::RaycastInfo& info, float rayLength);

class EngineRaycastCallback : public reactphysics3d::RaycastCallback
{
public:
	virtual void Raycast(reactphysics3d::PhysicsWorld* physicsWorld, XMVECTOR start, XMVECTOR end, CollisionLayers collisionLayers = CollisionLayers::All) {}
};

class AllRaycastCallback : public EngineRaycastCallback
{
public:
	FixedList<CollisionRecord> collisions;

private:
	float rayLength = 0.f;

public:
	AllRaycastCallback(MemoryArena& memoryArena) : collisions(memoryArena, MAX_COLLISION_RESULTS) {}

	virtual void Raycast(reactphysics3d::PhysicsWorld* physicsWorld, XMVECTOR start, XMVECTOR end, CollisionLayers collisionLayers = CollisionLayers::All) override
	{
		collisions.clear();
		rayLength = XMVectorGetX(XMVector3Length(end - start));
		reactphysics3d::Ray ray = reactphysics3d::Ray(PhysicsVectorFromXM(start), PhysicsVectorFromXM(end));
		physicsWorld->raycast(ray, this, static_cast<uint32_t>(collisionLayers));
	}

	virtual reactphysics3d::decimal notifyRaycastHit(const reactphysics3d::RaycastInfo& info) override
	{
		CollisionRecord& collision = collisions.newElement();
		SetCollisionRecord(collision, info, rayLength);
		return 1.;
	}
};

class MinimumRaycastCallback : public EngineRaycastCallback
{
public:
	bool anyCollision = false;
	CollisionRecord collision;

private:
	float rayLength = 0.f;

public:
	virtual void Raycast(reactphysics3d::PhysicsWorld* physicsWorld, XMVECTOR start, XMVECTOR end, CollisionLayers collisionLayers = CollisionLayers::All) override
	{
		anyCollision = false;
		rayLength = XMVectorGetX(XMVector3Length(end - start));
		reactphysics3d::Ray ray = reactphysics3d::Ray(PhysicsVectorFromXM(start), PhysicsVectorFromXM(end));
		physicsWorld->raycast(ray, this, static_cast<uint32_t>(collisionLayers));
	}

	virtual reactphysics3d::decimal notifyRaycastHit(const reactphysics3d::RaycastInfo& info) override
	{
		anyCollision = true;
		SetCollisionRecord(collision, info, rayLength);
		return info.hitFraction;
	}
};
