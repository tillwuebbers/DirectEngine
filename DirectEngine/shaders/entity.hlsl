SamplerState rawSampler : register(s0);
SamplerState smoothSampler : register(s1);

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
};

Texture2D diffuseTexture : register(t2);
Texture2D shadowmapTexture : register(t4);

cbuffer EntityConstantBuffer : register(b5)
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
	float4 worldPosition : POSITION;
	float4 lightSpacePosition : TEXCOORD1;
	float4 color : COLOR;
	float3 worldNormal : NORMAL;
	float2 uv : UV;
};

// Default material
PSInput VSMain(float4 position : POSITION, float4 vertColor : COLOR, float3 normal : NORMAL, float2 uv : UV)
{
	PSInput result;

	float4 worldPos = mul(position, worldTransform);
	result.worldPosition = worldPos;

	float4x4 vp = mul(cameraTransform, perspectiveTransform);
	result.position = mul(worldPos, vp);

	float4x4 lightVP = mul(lightTransform, lightPerspective);
	result.lightSpacePosition = mul(float4(worldPos.xyz, 1.0), lightVP);

	result.color = color;
	if (isSelected)
	{
		result.color = float4(1., 0., 1., 1.);
	}

	result.worldNormal = mul(float4(normal, 0.), worldTransform).xyz;
	result.uv = uv;

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

	// Specular (TODO: fucked)
	float3 viewDir = normalize(worldCameraPos - input.worldPosition.xyz);
	float3 halfwayDir = normalize(-sunDirection + viewDir);
	float specularIntensity = pow(max(dot(input.worldNormal, -halfwayDir), 0.), 32);
	float3 specularLightColor = float3(1., 1., 1.);
	float3 specularLit = specularIntensity * specularLightColor;

	// Texture
	input.uv.y = 1. - input.uv.y;
	float4 diffuseTex = diffuseTexture.Sample(smoothSampler, input.uv);

	// Shadow
	float2 shadowMapPos;
	shadowMapPos.x =  input.lightSpacePosition.x / input.lightSpacePosition.w * .5f + .5f;
	shadowMapPos.y = -input.lightSpacePosition.y / input.lightSpacePosition.w * .5f + .5f;

	float shadow = 0.;
	if (shadowMapPos.x >= 0. && shadowMapPos.x <= 1. && shadowMapPos.y >= 0. && shadowMapPos.y <= 1.)
	{
		float closestDepth = shadowmapTexture.Sample(rawSampler, shadowMapPos).r;
		float currentDepth = input.lightSpacePosition.z / input.lightSpacePosition.w - .002;
		shadow = currentDepth > closestDepth ? 1.0 : 0.0;
	}

	diffuseLit *= 1. - shadow;
	specularLit *= 1. - shadow;

	return float4((ambientLit + diffuseLit + specularLit) * diffuseTex.rgb * input.color.rgb, 1.);
}
