#include "common.hlsl"

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, uv);
	result.color = float4(1., 0., 0., 1.);
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
	LightData lightData = PSCalcLightData(input, .2f, 1.f, .0f, 32);
	float3 litColor = lightData.ambientLight + lightData.diffuseLight + lightData.specularLight;

	float3 appliedColor = float3(1., .1, .1) * litColor;

	return float4(PostProcess(appliedColor, input.worldPosition.rgb), 1.);
}
