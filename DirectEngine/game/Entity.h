#pragma once

#include "../core/EngineCore.h"
#include "../core/Audio.h"
#include "../core/Collision.h"
#include "Mesh.h"

#include <DirectXMath.h>
using namespace DirectX;

struct FixedStr
{
	constexpr static size_t SIZE = 128;
	char str[SIZE]{};

	FixedStr& operator=(const char* other)
	{
		strcpy_s((char*)str, SIZE, other);
		return *this;
	}

	FixedStr(const char* other)
	{
		strcpy_s((char*)str, SIZE, other);
	}

	bool operator==(const FixedStr& other) const
	{
		return strcmp(str, other.str) == 0;
	}

	bool operator==(const char* other) const
	{
		return strcmp(str, other) == 0;
	}
};

template <>
struct std::formatter<FixedStr> {
	constexpr auto parse(std::format_parse_context& ctx) {
		return ctx.begin();
	}

	auto format(const FixedStr& obj, std::format_context& ctx) {
		return std::format_to(ctx.out(), "{}", obj.str);
	}
};

class Entity
{
public:
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
	XMVECTOR scale{ 1.f, 1.f, 1.f };

	MAT_RMAJ localMatrix;
	MAT_RMAJ worldMatrix;

	FixedStr name = "Entity";

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

	XMVECTOR velocity = {};

	bool isGizmoTranslationArrow = false;
	XMVECTOR gizmoTranslationAxis{};

	bool isGizmoRotationRing = false;
	XMVECTOR gizmoRotationAxis{};

	bool isGizmoScaleCube = false;
	XMVECTOR gizmoScaleAxis{};

	AudioSource audioSource;

	void AddChild(Entity* child);
	void RemoveChild(Entity* child);

	EntityData& GetData();
	EntityConstantBuffer& GetBuffer();
	void UpdateWorldMatrix();
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
