#include "common.hlsl"

Texture2D diffuseTexture : register(t4);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, uv);

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
