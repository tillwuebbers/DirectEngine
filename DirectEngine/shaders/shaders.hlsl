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
    float4 color;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    uint id : SV_InstanceID;
};

PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, uint id : SV_InstanceID)
{
    PSInput result;

    float4 worldPos = mul(position, worldTransform);

    float4x4 vp = mul(cameraTransform, clipTransform);
    result.position = mul(worldPos, vp);
    
    result.color = color;
    result.id = id;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
