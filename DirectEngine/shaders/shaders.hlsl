cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 cameraTransform;
    float4x4 clipTransform;
    float time;
    float deltaTime;
};

cbuffer EntityConstantBuffer : register(b1)
{
    float4x4 worldTransform[8];
    float4 color[8];
    bool isSelected[8];
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, uint entityID : ENTITY_ID)
{
    PSInput result;

    float4 worldPos = mul(position, worldTransform[entityID]);

    float4x4 vp = mul(cameraTransform, clipTransform);
    result.position = mul(worldPos, vp);
    
    result.color = color[entityID];
    if (isSelected[entityID])
    {
        result.color = float4(1., 0., 1., 1.);// += float4(.1, .1, .1, .1);
    }

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
