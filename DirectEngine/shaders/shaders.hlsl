cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 cameraTransform;
	float4x4 clipTransform;
	float3 sunDirection;
	float3 worldCameraPos;
	float time;
	float deltaTime;
};

cbuffer EntityConstantBuffer : register(b1)
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
};

PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL)
{
	PSInput result;

	float4 worldPos = mul(position, worldTransform);
	result.worldPosition = worldPos;

	float4x4 vp = mul(cameraTransform, clipTransform);
	result.position = mul(worldPos, vp);

	result.color = color;
	if (isSelected)
	{
		result.color = float4(1., 0., 1., 1.);
	}

	result.worldNormal = mul(float4(normal, 0.), worldTransform).xyz;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float normalSunAngle = max(dot(input.worldNormal, -sunDirection), 0.);

	// Ambient
	float ambientIntensity = .2;
	float3 ambientLightColor = float3(1., 1., 1.);
	float3 ambientLit = ambientIntensity * ambientLightColor;

	// Diffuse
	float diffuseIntensity = normalSunAngle;
	float3 diffuseLightColor = float3(1., 1., 1.);
	float3 diffuseLit = diffuseIntensity * diffuseLightColor;

	// Specular
	float3 viewDir = normalize(input.worldPosition.xyz - worldCameraPos);
	float3 halfwayDir = normalize(sunDirection + viewDir);
	float specularIntensity = pow(max(dot(input.worldNormal, -halfwayDir), 0.), 256);
	float3 specularLightColor = float3(1., 1., 1.);
	float3 specularLit = specularIntensity * specularLightColor;

	return float4(ambientLit + diffuseLit + specularLit, 1.) * input.color;
}
