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

	EngineCore* engine;
	size_t materialIndex;
	size_t dataIndex;

	bool isSpinning = false;
	uint64_t collisionLayers = 0;

	EntityData* GetData();
	EntityConstantBuffer* GetBuffer();
	XMVECTOR LocalToWorld(XMVECTOR localPosition);
	XMVECTOR WorldToLocal(XMVECTOR worldPosition);
};
