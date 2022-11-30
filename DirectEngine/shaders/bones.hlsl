#include "common.hlsl"

cbuffer DrawConstants : register(b5)
{
	uint boneIndex;
};

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInputDefault result;

	const float displayScale = .2;
	float4 bonePos = mul(position * float4(displayScale, displayScale, displayScale, 1.), jointTransforms[boneIndex]);

	float4 worldPos = mul(bonePos, worldTransform);
	result.worldPosition = worldPos;

	float4x4 vp = mul(cameraView, cameraProjection);
	result.position = mul(worldPos, vp);

	float4x4 lightVP = mul(lightView, lightProjection);
	result.lightSpacePosition = mul(float4(worldPos.xyz, 1.0), lightVP);

	result.worldNormal = mul(float4(normal, 0.), worldTransform).xyz;
	result.uv = uv;

	result.color = float4(1., 0., 0., 1.);
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
	return float4(0., boneIndex / 100., 0., 1.);
}
