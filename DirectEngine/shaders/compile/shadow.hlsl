#include "util/common.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV, float4 boneWeights : BONE_WEIGHTS, uint4 boneIndices : BONE_INDICES)
{
	float4 pos = position;
	if (boneWeights[0] > 0.01)
	{
		float4 skinnedPos = boneWeights.x * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.x], jointTransforms[boneIndices.x])).xyz, 1.0);
		skinnedPos += boneWeights.y * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.y], jointTransforms[boneIndices.y])).xyz, 1.0);
		skinnedPos += boneWeights.z * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.z], jointTransforms[boneIndices.z])).xyz, 1.0);
		skinnedPos += boneWeights.w * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.w], jointTransforms[boneIndices.w])).xyz, 1.0);
		skinnedPos.w = 1.0;
		pos = skinnedPos;
	}

	PSInput result;

	float4 worldPos = mul(pos, worldTransform);
	result.position = mul(worldPos, mul(lightView, lightProjection));

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(0., 0., 0., 1.);
}