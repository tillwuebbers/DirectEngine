#include "common.hlsl"

Texture2D diffuseTexture : register(t5);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV, uint boneWeights: BONE_WEIGHTS, uint boneIndices: BONE_INDICES)
{
	float boneWeightArray[4];
	boneWeightArray[0] = (boneWeights & 0x000000FF) / 255.;
	boneWeightArray[1] = (boneWeights & 0x0000FF00) / 255.;
	boneWeightArray[2] = (boneWeights & 0x00FF0000) / 255.;
	boneWeightArray[3] = (boneWeights & 0xFF000000) / 255.;

	uint boneIndexArray[4];
	boneIndexArray[0] = boneIndices & 0x000000FF;
	boneIndexArray[1] = boneIndices & 0x0000FF00;
	boneIndexArray[2] = boneIndices & 0x00FF0000;
	boneIndexArray[3] = boneIndices & 0xFF000000;

	float4 skinnedPos = float4(0., 0., 0., 0.);
	for (int i = 0; i < 4; i++)
	{
		skinnedPos += boneWeightArray[i] * mul(position, boneMatrices[boneIndexArray[i]]);
	}

	PSInputDefault result = VSCalcDefault(skinnedPos, normal, uv);

	result.color = color;
	if (isSelected)
	{
		result.color = float4(1., 0., 1., 1.);
	}

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
