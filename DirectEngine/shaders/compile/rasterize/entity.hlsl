#include "util/common.hlsl"

Texture2D diffuseTexture : register(t6);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV)
{
    PSInputDefault result = VSCalcDefault(position, normal, tangent, bitangent, uv);
    result.color = color;
    return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    LightData lightData = PSCalcLightData(input, 0.1f, 1.0f, 1.0f, 32);
    float3 litColor = lightData.ambientLight + lightData.diffuseLight + lightData.specularLight;

    float4 diffuseTex = diffuseTexture.Sample(smoothSampler, input.uv);
    float3 appliedColor = litColor * diffuseTex.rgb * input.color.rgb;

    return float4(PostProcess(appliedColor, input.worldPosition.rgb), 1.);
}
