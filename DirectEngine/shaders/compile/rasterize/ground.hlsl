#include "util/common.hlsl"

Texture2D diffuseTexture : register(t6);
Texture2D normalTexture : register(t7);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float3 tangent: TANGENT, float3 bitangent: BITANGENT, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, tangent, bitangent, uv);
	result.color = color;
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
	float2 textureNormalSample = normalTexture.Sample(smoothSampler, input.uv).xy;
    float2 textureNormalXY = textureNormalSample * 2.0 - 1.0;
    float textureNormalZ = sqrt(saturate(1.0 - dot(textureNormalXY, textureNormalXY)));
	input.normal = float3(textureNormalXY, textureNormalZ);
	
    LightData lightData = PSCalcLightData(input, 0.1, 1.0, 1.0, 64);
	float3 litColor = lightData.ambientLight + lightData.diffuseLight + lightData.specularLight;
    float3 appliedColor = diffuseTexture.Sample(smoothSampler, input.uv).rgb * litColor;

	return float4(PostProcess(appliedColor, input.worldPosition.rgb), 1.);
}