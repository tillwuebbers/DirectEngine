#include "common.hlsl"

Texture2D diffuseTexture : register(t5);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV, uint boneWeights: BONE_WEIGHTS, uint boneIndices: BONE_INDICES)
{
	float boneWeightArray[4];
	boneWeightArray[0] = ( boneWeights        & 0x000000FF) / 255.;
	boneWeightArray[1] = ((boneWeights >> 8)  & 0x000000FF) / 255.;
	boneWeightArray[2] = ((boneWeights >> 16) & 0x000000FF) / 255.;
	boneWeightArray[3] = ((boneWeights >> 24) & 0x000000FF) / 255.;

	uint boneIndexArray[4];
	boneIndexArray[0] =  boneIndices        & 0x000000FF;
	boneIndexArray[1] = (boneIndices >> 8)  & 0x000000FF;
	boneIndexArray[2] = (boneIndices >> 16) & 0x000000FF;
	boneIndexArray[3] = (boneIndices >> 24) & 0x000000FF;

	float4 skinnedPos = float4(0., 0., 0., 0.);
	for (int i = 0; i < 4; i++)
	{
		uint boneIndex = boneIndexArray[i];
		float4x4 boneMatrix = boneMatrices[boneIndex];
		skinnedPos += boneWeightArray[i] * mul(position, boneMatrix);
	}

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
