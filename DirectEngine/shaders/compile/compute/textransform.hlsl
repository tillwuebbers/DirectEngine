Texture2D metal : register(t0);
Texture2D roughess : register(t1);
RWTexture2D<float4> result : register(u2);
SamplerState smoothSampler : register(s0);

struct TextureInfo
{
    uint2 size;
};
ConstantBuffer<TextureInfo> textureInfo : register(b0);

[numthreads(1, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    float2 uv = float2(threadID.xy) / float2(textureInfo.size);
    float3 metalSample = metal.SampleLevel(smoothSampler, uv, 0.0).xyz;
    float3 roughessSample = roughess.SampleLevel(smoothSampler, uv, 0.0).xyz;
    result[threadID.xy] = float4(0.0, metalSample.x, roughessSample.x, 1.0);
}