cbuffer SceneConstantBuffer : register(b0)
{
    float4 time;
    float4 padding[15];
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

    uint totalCount = 6000;
    uint countPerRow = 100;
    uint rowCount = totalCount / countPerRow;
    result.position = (position * float4(0.03, 0.03, 0.03, 1)) + float4((id % countPerRow) / (float)countPerRow * 1.8 - 0.9 + sin(time.x), (id / countPerRow) * (1. / rowCount) * 1.4 - 0.7, 0, 0);
    result.color = color;
    result.id = id;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    uint totalCount = 6000;
    return float4(input.id / (float)totalCount, .3, 0.1, 1);
}
