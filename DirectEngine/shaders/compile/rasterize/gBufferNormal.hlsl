#include "util/common.hlsl"

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV, float4 boneWeights : BONE_WEIGHTS, uint4 boneIndices : BONE_INDICES)
{
    PSInputDefault result = VSCalcDefault(position, normal, uv);
    
    result.color = vertColor;

    return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    return float4(input.worldNormal, 1.);
}