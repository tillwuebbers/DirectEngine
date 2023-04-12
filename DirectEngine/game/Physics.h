#pragma once
#include "Memory.h"
#include "Log.h"
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
	CollisionLayers ownCollisionLayers = CollisionLayers::Floor;
	CollisionLayers collidesWithLayers = CollisionLayers::All;
};

struct PhysicsDebugDrawer : btIDebugDraw
{
	int debugMode = 0;
	RingLog* log = nullptr;
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