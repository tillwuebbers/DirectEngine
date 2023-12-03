#include "util/common.hlsl"

TextureCube<float4> skybox : register(t0, space1);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 worldPosition : POSITION;
};

PSInput VSMain(float4 position : POSITION, float2 uv : UV)
{
    PSInput result;
    
    result.worldPosition = mul(position, worldTransform);
    result.position = mul(result.worldPosition, VSGetVP());
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 dir = normalize(input.worldPosition.xyz - worldCameraPos);
    float3 sampleColor = skybox.Sample(smoothSampler, dir).xyz;
    return float4(PostProcess(sampleColor, input.worldPosition.rgb), 1.0);
}
