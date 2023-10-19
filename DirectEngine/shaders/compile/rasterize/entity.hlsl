#include "util/common.hlsl"

Texture2D diffuseTexture : register(t6);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV, float4 boneWeights: BONE_WEIGHTS, uint4 boneIndices: BONE_INDICES)
{
	float4 skinnedPos = boneWeights.x * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.x], jointTransforms[boneIndices.x])).xyz, 1.0);
	skinnedPos       += boneWeights.y * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.y], jointTransforms[boneIndices.y])).xyz, 1.0);
	skinnedPos       += boneWeights.z * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.z], jointTransforms[boneIndices.z])).xyz, 1.0);
	skinnedPos       += boneWeights.w * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.w], jointTransforms[boneIndices.w])).xyz, 1.0);
	skinnedPos.w = 1.0;

	PSInputDefault result = VSCalcDefault(skinnedPos, normal, uv);
	result.color = color;

	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    LightData lightData = PSCalcLightData(input, .2f, 1.f, .0f, 32);
	float3 litColor = lightData.ambientLight + lightData.diffuseLight + lightData.specularLight;

	float4 diffuseTex = diffuseTexture.Sample(smoothSampler, input.uv);
	float3 appliedColor = litColor * diffuseTex.rgb * input.color.rgb;

	return float4(PostProcess(appliedColor, input.worldPosition.rgb), 1.);
}
