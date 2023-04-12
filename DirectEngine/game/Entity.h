#pragma once

#include "../core/Memory.h"
#include "../core/EngineCore.h"
#include "../core/Audio.h"
#include "Mesh.h"
#include "Physics.h"

#include <DirectXMath.h>
using namespace DirectX;

class Entity : btMotionState
{
public:
	ExtendedMatrix localMatrix;
	ExtendedMatrix worldMatrix;
	
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

	bool isNearPortal1 = false;
	bool isNearPortal2 = false;

	btRigidBody* rigidBody = nullptr;
	btDynamicsWorld* rigidBodyWorld = nullptr;
	XMVECTOR physicsShapeOffset = {};

	AudioSource audioSource;

	void AddChild(Entity* child, bool keepWorldPosition);
	void RemoveChild(Entity* child, bool keepWorldPosition);

	void AddRigidBody(MemoryArena& arena, btDynamicsWorld* world, btCollisionShape* shape, PhysicsInit& physicsInit);

	EntityData& GetData();
	EntityConstantBuffer& GetBuffer();
	void UpdateWorldMatrix();
	void SetForwardDirection(XMVECTOR direction, XMVECTOR up = V3_UP, XMVECTOR altUp = V3_RIGHT);
	XMVECTOR GetForwardDirection() const;
	void UpdateAudio(EngineCore& engine, const X3DAUDIO_LISTENER* audioListener);
	void UpdateAnimation(EngineCore& engine, bool isMainRender);
	void SetActive(bool newState, bool affectSelf = true);
	bool IsActive();

	void SetLocalPosition(const XMVECTOR localPos);
	void SetLocalRotation(const XMVECTOR localRot);
	void SetLocalScale(const XMVECTOR localScale);
	XMVECTOR GetLocalPosition() const;
	XMVECTOR GetLocalRotation() const;
	XMVECTOR GetLocalScale() const;

	void SetWorldPosition(const XMVECTOR worldPos);
	void SetWorldRotation(const XMVECTOR worldRot);
	void SetWorldScale(const XMVECTOR worldScale);
	XMVECTOR GetWorldPosition() const;
	XMVECTOR GetWorldRotation() const;
	XMVECTOR GetWorldScale() const;

	void getWorldTransform(btTransform& worldTrans) const;
	void setWorldTransform(const btTransform& worldTrans);

private:
	bool isParentActive = true;
	bool isSelfActive = true;
};

XMVECTOR SampleAnimation(AnimationData& animData, float animationTime, XMVECTOR(__vectorcall* interp)(XMVECTOR a, XMVECTOR b, float t));
