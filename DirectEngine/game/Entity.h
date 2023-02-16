#pragma once

#include "../core/EngineCore.h"

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

	const char* name = "Entity";
	bool isActive = true;

	EngineCore* engine;

	Entity* parent = nullptr;
	Entity* children[MAX_ENTITY_CHILDREN];
	size_t childCount = 0;

	bool isRendered = false;
	bool isSkinnedMesh = false;
	size_t materialIndex;
	size_t dataIndex;

	bool isSkinnedRoot = false;
	TransformHierachy* transformHierachy;

	bool checkForShadowBounds = true;
	XMVECTOR aabbLocalPosition = { 0., 0., 0. };
	XMVECTOR aabbLocalSize = { 1., 1., 1. };
	uint64_t collisionLayers = 0;

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
	XMVECTOR LocalToWorld(XMVECTOR localPosition);
	XMVECTOR WorldToLocal(XMVECTOR worldPosition);
	void Disable();
};
