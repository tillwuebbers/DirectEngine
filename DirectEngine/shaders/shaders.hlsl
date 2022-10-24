cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 modelTransform;
    float4x4 cameraTransform;
    float4x4 clipTransform;
    float time;
    float deltaTime;
    float padding[3];
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    uint id : SV_InstanceID;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, uint id : SV_InstanceID)
{
    PSInput result;

    float4x4 mvp = mul(modelTransform, mul(cameraTransform, clipTransform));
    result.position = mul(position, mvp);
    result.position.x += id * 1.5;
    
    result.color = color;
    result.id = id;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    uint totalCount = 6000;
    return float4(input.id / (float)totalCount, .3, 0.1, 1);
}
