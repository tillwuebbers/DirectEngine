#pragma once

#include "../core/Memory.h"
#include "../core/EngineCore.h"
#include "../core/Audio.h"
#include "../core/Collision.h"
#include "Mesh.h"

#include <DirectXMath.h>
using namespace DirectX;

#include <reactphysics3d/reactphysics3d.h>

class Entity
{
public:
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	XMVECTOR scale{ 1.f, 1.f, 1.f };

	MAT_RMAJ localMatrix = XMMatrixIdentity();
	MAT_RMAJ worldMatrix = XMMatrixIdentity();

	FixedStr name = "Entity";

	EngineCore* engine;

	Entity* parent = nullptr;
	CountingArray<Entity*, MAX_ENTITY_CHILDREN> children = {};

	bool isRendered = false;
	bool isSkinnedMesh = false;
	size_t materialIndex;
	size_t dataIndex;

	bool isSkinnedRoot = false;
	TransformHierachy* transformHierachy = nullptr;

	bool isGizmoTranslationArrow = false;
	XMVECTOR gizmoTranslationAxis{};

	bool isGizmoRotationRing = false;
	XMVECTOR gizmoRotationAxis{};

	bool isGizmoScaleCube = false;
	XMVECTOR gizmoScaleAxis{};

	reactphysics3d::RigidBody* rigidBody = nullptr;
	reactphysics3d::CollisionBody* collisionBody = nullptr;

	AudioSource audioSource;

	void AddChild(Entity* child, bool keepWorldPosition);
	void RemoveChild(Entity* child, bool keepWorldPosition);

	reactphysics3d::Transform GetPhysicsTransform();
	void InitRigidBody(reactphysics3d::PhysicsWorld* physicsWorld, reactphysics3d::BodyType type = reactphysics3d::BodyType::DYNAMIC);
	void InitCollisionBody(reactphysics3d::PhysicsWorld* physicsWorld);
	reactphysics3d::Collider* InitBoxCollider(reactphysics3d::PhysicsCommon& physicsCommon, XMVECTOR boxExtents, XMVECTOR boxOffset, CollisionLayers collisionLayers, float bounciness = 0.1f, float friction = 0.5f, float density = 1.0f);

	EntityData& GetData();
	EntityConstantBuffer& GetBuffer();
	void UpdateWorldMatrix();
	void SetForwardDirection(XMVECTOR direction, XMVECTOR up = V3_UP, XMVECTOR altUp = V3_RIGHT);
	XMVECTOR GetForwardDirection();
	void UpdateAudio(EngineCore& engine, const X3DAUDIO_LISTENER* audioListener);
	void UpdateAnimation(EngineCore& engine, bool isMainRender);
	void SetActive(bool newState, bool affectSelf = true);
	bool IsActive();

private:
	bool isParentActive = true;
	bool isSelfActive = true;
};

void CalculateDirectionVectors(XMVECTOR& outForward, XMVECTOR& outRight, XMVECTOR& outUp, XMVECTOR inRotation);

XMVECTOR SampleAnimation(AnimationData& animData, float animationTime, XMVECTOR(__vectorcall* interp)(XMVECTOR a, XMVECTOR b, float t));
