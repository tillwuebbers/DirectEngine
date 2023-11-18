#include "util/common.hlsl"

struct RootConstants
{
    float layerOffset;
    uint layerCount;
};
ConstantBuffer<RootConstants> rootConstants : register(b6);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD_0;
    uint instanceID : SV_InstanceID;
};

PSInput VSMain(float4 position : POSITION, float3 normal : NORMAL, float2 uv : UV, uint instanceID : SV_InstanceID)
{
    PSInput result;
    float3 newPos = position.xyz + normal * instanceID * rootConstants.layerOffset;
    float4 worldPos = mul(float4(newPos, 1.0), worldTransform);
    result.position = mul(worldPos, VSGetVP());
    result.uv = uv;
    result.instanceID = instanceID;
    return result;
}

float hash(uint n) {
    // integer hash copied from Hugo Elias
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 0x789221U) + 0x1376312589U;
    return float(n & uint(0x7fffffffU)) / float(0x7fffffff);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 newUV = input.uv * 100.0;
    float2 localUV = frac(newUV) * 2.0 - 1.0;
    
    uint2 seedInput = newUV;
    uint seed = seedInput.x + 100 * seedInput.y * 100 + 1000;
    float rand = lerp(0.2, 1.0, hash(seed));

    float centerDistance = length(localUV);
    float normalizedShellHeight = float(input.instanceID) / float(rootConstants.layerCount);
    float sdf = centerDistance - (rand - normalizedShellHeight);

    if (input.instanceID > 0 && sdf > 0.5)
    {
        discard;
    }

    float3 color = float3(1.0, 1.0, 1.0);
    float ambientOcclusion = pow(normalizedShellHeight, 0.5);
    return float4(color * ambientOcclusion, 1.0);
}
