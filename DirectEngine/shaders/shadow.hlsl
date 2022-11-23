#include "common.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : UV;
};

PSInput VSShadow(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;

	float4 worldPos = mul(position, worldTransform);

	float4x4 vp;
	vp = mul(lightView, lightProjection);
	result.position = mul(worldPos, vp);

	result.uv = uv;

	return result;
}

float4 PSShadow(PSInput input) : SV_TARGET
{
	return float4(0., 0., 0., 1.);
}