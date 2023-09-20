#include "util/common.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;

	float3 aabbPosition = position.xyz * aabbLocalSize + aabbLocalPosition;
	float4 worldPos = mul(float4(aabbPosition, position.w), worldTransform);
	result.position = mul(worldPos, mul(cameraView, cameraProjection));

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(0., 1., 1., 1.);
}