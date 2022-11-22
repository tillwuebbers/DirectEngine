#include "common.hlsl"

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

	float3 baseColor = float3(.02, .02, .02);

	float2 gridIn = (input.uv + float2(frac(time.x * .1), 0.)) * 100.;
	float2 gridCell = floor(gridIn);
	float3 randomGridColor = float3(RandomPerPixel(gridCell, 43.1 + floor(time.x * 2.)), RandomPerPixel(gridCell, 24.34), RandomPerPixel(gridCell, 33.3));

	return float4(randomGridColor * litColor * baseColor, 1.);
}
