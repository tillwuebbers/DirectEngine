#include "util/common.hlsl"

PSInputDefault VSMain(float4 position : POSITION, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV)
{
    PSInputDefault result = VSCalcDefault(position, normal, tangent, bitangent, uv);
    return result;
}

void PSMain() : SV_TARGET
{
}
