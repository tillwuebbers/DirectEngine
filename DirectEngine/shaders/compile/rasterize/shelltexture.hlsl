#include "util/common.hlsl"

struct RootConstants
{
    uint layer;
    float layerOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b6);

PSInputDefault VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV)
{
    float3 newPos = position.xyz + normal * rootConstants.layer * rootConstants.layerOffset;
    PSInputDefault result = VSCalcDefault(float4(newPos, position.w), normal, tangent, bitangent, uv);
    result.color = color;
    return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    return float4(1.0, 1.0, 0.0, 1.0);
}
