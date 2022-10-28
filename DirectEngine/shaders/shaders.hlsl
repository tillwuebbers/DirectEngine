cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 cameraTransform;
    float4x4 clipTransform;
    float time;
    float deltaTime;
};

cbuffer EntityConstantBuffer : register(b1)
{
    float4x4 worldTransform;
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

    float4 worldPos = mul(position, worldTransform);
    worldPos.x += (id % 10) * 1.5 - 5. * 1.3333;
    worldPos.y += (id / 10) * 1.5 - 5. * 1.3333;

    float4x4 vp = mul(cameraTransform, clipTransform);
    result.position = mul(worldPos, vp);
    
    result.color = color;
    result.id = id;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    uint totalCount = 100;
    return float4(input.id / (float)totalCount, .3, 0.1, 1);
}
