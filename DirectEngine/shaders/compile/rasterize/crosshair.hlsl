#include "util/common.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 uv : UV;
};

PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;
	result.position = float4(mul(mul(position, worldTransform), cameraProjection).xy, 0.f, 1.f);
	result.color = color;
	result.uv = uv;
	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float hDistance = abs(input.uv.x - 0.5);
	float vDistance = abs(input.uv.y - 0.5);

	if (hDistance < 0.05 && vDistance > 0.15) return input.color;
	if (hDistance > 0.15 && vDistance < 0.05) return input.color;

	float2 center = float2(0.5f, 0.5f);
	float2 toCenter = center - input.uv;
	float dist = length(toCenter);
	
	if (dist > 0.08)
	{
		discard;
	}
	
	return input.color;
}
