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

float hash(uint n) {
    // integer hash copied from Hugo Elias
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 0x789221U) + 0x1376312589U;
    return float(n & uint(0x7fffffffU)) / float(0x7fffffff);
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    float2 newUV = input.uv * 100.0;
    float2 localUV = frac(newUV) * 2.0 - 1.0;
    
    uint2 seedInput = newUV;
    uint seed = seedInput.x + 100 * seedInput.y * 100 + 1000;
    float rand = lerp(0.2, 1.0, hash(seed));

    float centerDistance = length(localUV);
    float normalizedShellHeight = float(rootConstants.layer) / 32.0;
    float sdf = centerDistance - (rand - normalizedShellHeight);

    if (rootConstants.layer > 0 && sdf > 0.5)
    {
        discard;
    }

    float3 color = float3(1.0, 1.0, 1.0);
    float ambientOcclusion = pow(normalizedShellHeight, 0.5);
    return float4(color * ambientOcclusion, 1.0);
}
