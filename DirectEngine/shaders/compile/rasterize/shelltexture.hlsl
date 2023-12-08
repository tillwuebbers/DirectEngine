#include "util/common.hlsl"

struct RootConstants
{
    float layerOffset;
    uint layerCount;
};
ConstantBuffer<RootConstants> rootConstants : register(b0, space1);

void VSMain(float4 position : POSITION, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV, out PSInputDefault result, inout uint instanceID : SV_InstanceID)
{
    // Offset the position based on the layer
    float3 newPos = position.xyz + normal * instanceID * rootConstants.layerOffset;
    result = VSCalcDefault(float4(newPos, 1.0), normal, tangent, bitangent, uv);
}

// integer hash from Hugo Elias
// Generates a random number between 0 and 1 for a given seed
float intHash(uint n) {
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 0x789221U) + 0x1376312589U;
    return float(n & uint(0x7fffffffU)) / float(0x7fffffff);
}

float4 PSMain(PSInputDefault input, uint instanceID : SV_InstanceID) : SV_TARGET
{
    float2 newUV = input.uv * 5000.0;
    float2 localUV = frac(newUV) * 2.0 - 1.0;
    
    uint2 seedInput = newUV;
    uint seed = seedInput.x + 100 * seedInput.y * 100 + 1000;
    float rand = lerp(0.2, 1.0, intHash(seed));

    float centerDistance = length(localUV);
    float normalizedShellHeight = float(instanceID) / float(rootConstants.layerCount);
    float distanceFunction = centerDistance - (rand - normalizedShellHeight);

    if (instanceID > 0 && distanceFunction > 0.5)
    {
        discard;
    }
    
    float ambientOcclusion = pow(normalizedShellHeight, 0.5);
    float3 color = PBR(input, float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 1.0), 0.5, 0.0);
    return float4(PostProcess(color, input.worldPosition.rgb), 1.0) * ambientOcclusion;
}
