#include "util/common.hlsl"

struct PSInput
{
    float4 position : SV_POSITION;
    float4 worldPosition : POSITION;
    float4 worldNormal : NORMAL;
};

struct PSOutput
{
    float4 normal : SV_Target0;
    float4 worldPosition : SV_Target1;
};

PSInput VSMain(float4 position : POSITION, float3 normal : NORMAL, float2 uv : UV, float4 boneWeights : BONE_WEIGHTS, uint4 boneIndices : BONE_INDICES)
{
    PSInput result;
    
    result.worldPosition = mul(position, worldTransform);
    result.position = mul(result.worldPosition, VSGetVP());
    result.worldNormal = mul(float4(normal, 0.), worldTransform);

    return result;
}

PSOutput PSMain(PSInput input)
{
    PSOutput result;
    result.normal = float4(input.worldNormal.xyz, input.position.z);
    result.worldPosition = float4(input.worldPosition.xyz - worldCameraPos, 1.0);
    return result;
}