SamplerState textureSampler : register(s0);

cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 cameraTransform;
	float4x4 perspectiveTransform;
	float3 worldCameraPos;
	float time;
	float deltaTime;
};

cbuffer LightConstantBuffer : register(b1)
{
	float4x4 lightTransform;
	float4x4 lightPerspective;
	float3 sunDirection;
	bool shadowPassActive;
};

cbuffer EntityConstantBuffer : register(b5)
{
	float4x4 worldTransform;
	float4 color;
	bool isSelected;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float4 worldPosition : POSITION;
	float4 color : COLOR;
	float3 worldNormal : NORMAL;
	float2 uv : UV;
};

PSInput VSShadow(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;

	float4 worldPos = mul(position, worldTransform);
	result.worldPosition = worldPos;

	float4x4 vp;
	vp = mul(lightTransform, lightPerspective);
	result.position = mul(worldPos, vp);

	result.color = color;

	result.worldNormal = mul(float4(normal, 0.), worldTransform).xyz;
	result.uv = uv;

	return result;
}

float4 PSShadow(PSInput input) : SV_TARGET
{
	return input.color;
}