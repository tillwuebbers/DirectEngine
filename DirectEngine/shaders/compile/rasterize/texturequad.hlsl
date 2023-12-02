#include "util/common.hlsl"

Texture2D diffuseTexture : register(t0, space1);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, tangent, bitangent, uv);
	result.color = color;
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
	float4 diffuseColor = diffuseTexture.Sample(smoothSampler, float2(1. - input.uv.x, input.uv.y));
	return float4(PostProcess(diffuseColor.rgb, input.worldPosition.rgb), 1.);
}
