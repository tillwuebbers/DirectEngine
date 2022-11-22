cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 cameraTransform;
	float4x4 perspectiveTransform;
	float3 worldCameraPos;
	float time;
	float deltaTime;
};

cbuffer EntityConstantBuffer : register(b2)
{
	float4x4 worldTransform;
	float4 color;
	float3 aabbLocalPosition;
	float3 aabbLocalSize;
	bool isSelected;
};

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput VSWire(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;

	float4x4 aabbTransform = worldTransform;
	aabbTransform[0][0] *= aabbLocalSize.x;
	aabbTransform[1][1] *= aabbLocalSize.y;
	aabbTransform[2][2] *= aabbLocalSize.z;

	float4 worldPos = mul(position, aabbTransform);
	float4 worldOffset = mul(float4(aabbLocalPosition, 0.), worldTransform);

	float4x4 vp = mul(cameraTransform, perspectiveTransform);
	result.position = mul(worldPos + worldOffset, vp);

	return result;
}

float4 PSWire(PSInput input) : SV_TARGET
{
	return float4(0., 1., 1., 1.);
}