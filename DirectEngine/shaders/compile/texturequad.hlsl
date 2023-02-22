#include "util/common.hlsl"

Texture2D diffuseTexture : register(t7);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, uv);
	result.color = color;
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
	float4 diffuseColor = diffuseTexture.Sample(smoothSampler, input.uv);
	return float4(PostProcess(diffuseColor, input.worldPosition.rgb), 1.);
}
