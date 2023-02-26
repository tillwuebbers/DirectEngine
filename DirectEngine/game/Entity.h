#pragma once

#include "../core/EngineCore.h"
#include "../core/Audio.h"
#include "../core/Collision.h"

#include <DirectXMath.h>
using namespace DirectX;

class Entity
{
public:
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	XMVECTOR scale{ 1.f, 1.f, 1.f };

	MAT_RMAJ localMatrix;
	MAT_RMAJ worldMatrix;

	std::string name = "Entity";

	EngineCore* engine;

	Entity* parent = nullptr;
	Entity* children[MAX_ENTITY_CHILDREN];
	size_t childCount = 0;

	bool isRendered = false;
	bool isSkinnedMesh = false;
	size_t materialIndex;
	size_t dataIndex;

	bool isSkinnedRoot = false;
	TransformHierachy* transformHierachy = nullptr;
	CollisionData* collisionData = nullptr;

	bool isEnemy = false;
	bool isProjectile = false;
	float spawnTime = -1000.f;
	XMVECTOR velocity = {};

	AudioSource audioSource;

	void AddChild(Entity* child);
	void RemoveChild(Entity* child);

	EntityData& GetData();
	EntityConstantBuffer& GetBuffer();
	void UpdateWorldMatrix();
	void UpdateAudio(EngineCore& engine, const X3DAUDIO_LISTENER* audioListener);
	void UpdateAnimation(EngineCore& engine);
	XMVECTOR LocalToWorld(XMVECTOR localPosition);
	XMVECTOR WorldToLocal(XMVECTOR worldPosition);
	void SetActive(bool newState);
	bool IsActive();

private:
	bool isActive = true;
};

void CalculateDirectionVectors(XMVECTOR& outForward, XMVECTOR& outRight, XMVECTOR& outUp, XMVECTOR inRotation);

XMVECTOR SampleAnimation(AnimationData& animData, float animationTime, XMVECTOR(__vectorcall* interp)(XMVECTOR a, XMVECTOR b, float t));
