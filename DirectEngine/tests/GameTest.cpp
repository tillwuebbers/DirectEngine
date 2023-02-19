#include "TestCommon.h"

#include "../game/Entity.h"
#include "../game/Game.h"

TEST(Animation, Sample)
{
	float times[] = { 0.f, 0.25f, 0.75f, 1.f };
	XMVECTOR data[] = { XMVectorSet(0.f, 0.f, 0.f, 0.f), XMVectorSet(1.f, 1.f, 1.f, 1.f), XMVectorSet(2.f, 2.f, 2.f, 2.f), XMVectorSet(3.f, 3.f, 3.f, 3.f) };
	AnimationData animData{};
	animData.frameCount = _countof(times);
	animData.times = times;
	animData.data = data;

	AssertVectorEqual(SampleAnimation(animData, -1.f, XMVectorLerp), { 0.f, 0.f, 0.f, 0.f });
	AssertVectorEqual(SampleAnimation(animData, 0.f, XMVectorLerp), { 0.f, 0.f, 0.f, 0.f });
	AssertVectorEqual(SampleAnimation(animData, 0.25f, XMVectorLerp), { 1.f, 1.f, 1.f, 1.f });
	AssertVectorEqual(SampleAnimation(animData, 0.5f, XMVectorLerp), { 1.5f, 1.5f, 1.5f, 1.5f });
	AssertVectorEqual(SampleAnimation(animData, 0.75f, XMVectorLerp), { 2.f, 2.f, 2.f, 2.f });
	AssertVectorEqual(SampleAnimation(animData, 1.f, XMVectorLerp), { 3.f, 3.f, 3.f, 3.f });
	AssertVectorEqual(SampleAnimation(animData, 1.25f, XMVectorLerp), { 3.f, 3.f, 3.f, 3.f });
}

TEST(Shadows, ShadowSpaceBasic)
{
	// directx coordinate system: +x is right, +y is up, +z is forward

	MAT_RMAJ camView = XMMatrixIdentity(); // cam is looking down the +z axis
	MAT_RMAJ camProj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.f, 0.1f, 100.f);
	MAT_RMAJ lightView = XMMatrixIdentity(); // light is looking down the +z axis
	XMMATRIX result = CalculateShadowCamProjection(camView, camProj, lightView);
	XMMATRIX expected{
		{.01f, .0f,  .0f,    .0f},
		{.0f,  .01f, .0f,    .0f},
		{.0f,  .0f,  .01f,   .0f},
		{.0f,  .0f, -.001f, 1.0f}
	};
	AssertMatrixEqual(result, expected);
}

TEST(Shadows, ShadowSpaceTopDown)
{
	// directx coordinate system: +x is right, +y is up, +z is forward

	MAT_RMAJ camView = XMMatrixIdentity(); // cam is looking down the +z axis
	MAT_RMAJ camProj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.f, 0.1f, 100.f);
	// light is looking down the -y axis
	MAT_RMAJ lightView = XMMatrixRotationX(XM_PIDIV2);
	XMMATRIX result = CalculateShadowCamProjection(camView, camProj, lightView);
	XMMATRIX expected{
		{.01f,  .0f,    .0f,    .0f},
		{.0f,   .02f,   .0f,    .0f},
		{.0f,   .0f,    .005f,  .0f},
		{.0f,  1.002f,  .5f,   1.0f}
	};
	AssertMatrixEqual(result, expected);
}