#include "util/common.hlsl"

Texture2D diffuseTexture : register(t0, space1);

struct PSInput
{
	float4 position : SV_POSITION;
	float4 worldPosition : POSITION;
	float4 screenPosition : TEXCOORD0;
};

PSInput VSMain(float4 position : POSITION, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;
	result.worldPosition  = mul(position,             worldTransform);
	result.position       = mul(result.worldPosition, VSGetVP());
	result.screenPosition = CalcScreenPos(result.position * float4(1., -1., 1., 1.)); 
	
	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float2 uv = input.screenPosition.xy / input.screenPosition.w;
	float4 diffuseColor = diffuseTexture.Sample(smoothSampler, uv);
    return float4(diffuseColor.rgb, 1.);
}
