#include "util/common.hlsl"

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, tangent, bitangent, uv);
	result.color = color;
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
	float2 rng = (input.uv * 987.789) % 12.3 / 12.3;
	return float4(rng.x, rng.y, 0., 1.);
}
