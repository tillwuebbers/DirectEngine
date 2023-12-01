#include "util/common.hlsl"

Texture2D diffuseTexture : register(t6);
Texture2D normalTexture : register(t7);
Texture2D roughnessTexture : register(t8);
Texture2D metallicTexture : register(t9);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV)
{
    PSInputDefault result = VSCalcDefault(position, normal, tangent, bitangent, uv);
    result.color = color;
    return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    float3 albedoSample = diffuseTexture.Sample(smoothSampler, input.uv).xyz;
    float2 normalSample = normalTexture.Sample(smoothSampler, input.uv).xy * 2. - 1.;
    float normalZ = sqrt(saturate(1.0 - dot(normalSample, normalSample)));
    float3 normalTS = float3(normalSample, normalZ);
    float roughnessSample = roughnessTexture.Sample(smoothSampler, input.uv).x;
    float metallicSample = metallicTexture.Sample(smoothSampler, input.uv).x;
    float3 appliedColor = PBR(input, albedoSample, normalTS, roughnessSample + 0.0001, metallicSample);
    
    return float4(PostProcess(appliedColor, input.worldPosition.rgb), 1.);
}
