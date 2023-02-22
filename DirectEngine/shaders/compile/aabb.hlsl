#include "util/common.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;

	float4x4 aabbTransform = worldTransform;
	aabbTransform[0][0] *= aabbLocalSize.x;
	aabbTransform[1][1] *= aabbLocalSize.y;
	aabbTransform[2][2] *= aabbLocalSize.z;

	float4 worldPos = mul(position, aabbTransform);
	float4 worldOffset = mul(float4(aabbLocalPosition, 0.), worldTransform);

	result.position = mul(worldPos + worldOffset, mul(cameraView, cameraProjection));

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(0., 1., 1., 1.);
}