#pragma once
#include "Memory.h"
#include "../core/EngineCore.h"
#include "../Helpers.h"

#include <DirectXMath.h>
using namespace DirectX;

#include <vector>

#define BT_NO_SIMD_OPERATOR_OVERLOADS
#include "btBulletDynamicsCommon.h"

enum class PhysicsInitType
{
	None,
	RigidBodyDynamic,
	RigidBodyKinematic,
	RigidBodyStatic,
};

#define CollisionLayersTable\
  EnumFlag(1, CL_Player, "Player")\
  EnumFlag(2, CL_World,  "World")\
  EnumFlag(3, CL_Entity, "Entity")\
  EnumFlag(4, CL_Portal, "Portal")\
  EnumFlag(5, CL_Gizmo,  "Gizmo Click")

enum class CollisionLayers : uint32_t
{
	CL_None = 0U,
#define EnumFlag(idx, name, str) name = 1U << (idx - 1),
	CollisionLayersTable
#undef EnumFlag
	CL_All = ~0U
};

const std::vector<const char*> collisionLayerNames =
{
#define EnumFlag(idx, name, str) str,
	CollisionLayersTable
#undef EnumFlag
};

inline CollisionLayers operator|(CollisionLayers a, CollisionLayers b)
{
	return static_cast<CollisionLayers>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline CollisionLayers operator&(CollisionLayers a, CollisionLayers b)
{
	return static_cast<CollisionLayers>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

struct PhysicsInit
{
	PhysicsInit() = default;
	PhysicsInit(float mass, PhysicsInitType type) : mass(mass), type(type) {}

	float mass = 0.f;
	PhysicsInitType type = PhysicsInitType::None;
	CollisionLayers ownCollisionLayers = CollisionLayers::CL_Entity;
	CollisionLayers collidesWithLayers = CollisionLayers::CL_All;
};

struct PhysicsDebugDrawer : btIDebugDraw
{
	int debugMode = 0;
	EngineCore* engine = nullptr;

	void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
	void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override;
	void reportErrorWarning(const char* warningString) override;
	void draw3dText(const btVector3& location, const char* textString) override;
	void setDebugMode(int debugMode) override;
	int getDebugMode() const override;
};

inline btVector3 ToBulletVec3(XMVECTOR vec)
{
    return { XMVectorGetX(vec), XMVectorGetY(vec), XMVectorGetZ(vec) };
}

inline btQuaternion ToBulletQuat(XMVECTOR quat)
{
    return { XMVectorGetX(quat), XMVectorGetY(quat), XMVectorGetZ(quat), XMVectorGetW(quat) };
}

inline XMVECTOR ToXMVec(btVector3 vec)
{
    return { vec.x(), vec.y(), vec.z() };
}

inline XMVECTOR ToXMVec(btVector4 vec)
{
	return { vec.x(), vec.y(), vec.z(), vec.w() };
}

inline XMVECTOR ToXMQuat(btQuaternion quat)
{
    return { quat.x(), quat.y(), quat.z(), quat.w() };
}