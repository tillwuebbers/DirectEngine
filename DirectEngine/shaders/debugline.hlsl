#include "common.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput VSLine(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;
	
	result.position = mul(position, VSGetVP());
	result.color = vertColor;

	return result;
}

float4 PSLine(PSInput input) : SV_TARGET
{
	return input.color;
}