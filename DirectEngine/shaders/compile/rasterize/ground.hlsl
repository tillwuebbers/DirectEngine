#include "util/common.hlsl"

Texture2D diffuseTexture : register(t6);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, uv);

	result.color = color;
	
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    LightData lightData = PSCalcLightData(input, 0.1f, 1.0f, 1.0f, 64);
	float3 litColor = lightData.ambientLight + lightData.diffuseLight + lightData.specularLight;
	/*float3 baseColor = float3(.02, .02, .02);
	float2 gridCell = floor(input.uv * 2. + 102.);
	float3 randomGridColor = float3(RandomPerPixel(gridCell, 43.1), RandomPerPixel(gridCell, 24.34), RandomPerPixel(gridCell, 33.3));*/

    float3 appliedColor = diffuseTexture.Sample(smoothSampler, input.uv).rgb * litColor;

	return float4(PostProcess(appliedColor, input.worldPosition.rgb), 1.);
}
