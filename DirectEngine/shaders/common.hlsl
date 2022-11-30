SamplerState rawSampler : register(s0);
SamplerState smoothSampler : register(s1);

cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 cameraView;
	float4x4 cameraProjection;
	float4 postProcessing;
	float3 fogColor;
	float3 worldCameraPos;
	float4 time;
};

cbuffer LightConstantBuffer : register(b1)
{
	float4x4 lightView;
	float4x4 lightProjection;
	float3 sunDirection;
};

cbuffer EntityConstantBuffer : register(b2)
{
	float4x4 worldTransform;
	float4 color;
	float3 aabbLocalPosition;
	float3 aabbLocalSize;
	bool isSelected;
};

cbuffer BoneMatricesBuffer : register(b3)
{
	float4x4 inverseJointBinds[128];
	float4x4 jointTransforms[128];
};

Texture2D shadowmapTexture : register(t4);

struct PSInputDefault
{
	float4 position : SV_POSITION;
	float4 worldPosition : POSITION;
	float4 lightSpacePosition : TEXCOORD1;
	float4 color : COLOR;
	float3 worldNormal : NORMAL;
	float2 uv : UV;
};

PSInputDefault VSCalcDefault(float4 position, float3 normal, float2 uv)
{
	PSInputDefault result;

	float4 worldPos = mul(position, worldTransform);
	result.worldPosition = worldPos;

	float4x4 vp = mul(cameraView, cameraProjection);
	result.position = mul(worldPos, vp);

	float4x4 lightVP = mul(lightView, lightProjection);
	result.lightSpacePosition = mul(float4(worldPos.xyz, 1.0), lightVP);

	result.worldNormal = mul(float4(normal, 0.), worldTransform).xyz;
	result.uv = uv;
	return result;
}

struct LightData
{
	float3 ambientLight;
	float3 diffuseLight;
	float3 specularLight;
};

LightData PSCalcLightData(PSInputDefault input, float ambientIntensity, float diffuseIntensity, float specularIntensity, float specularity)
{
	LightData result;
	float normalSunAngle = max(dot(input.worldNormal, -sunDirection), 0.);

	// Ambient
	float3 ambientLightColor = float3(1., 1., 1.);
	result.ambientLight = ambientIntensity * ambientLightColor;

	// Diffuse
	float diffuseIntensityAdjusted = normalSunAngle * diffuseIntensity;
	float3 diffuseLightColor = float3(1., 1., 1.);
	result.diffuseLight = diffuseIntensityAdjusted * diffuseLightColor;

	// Specular (TODO: fucked)
	float3 viewDir = normalize(worldCameraPos - input.worldPosition.xyz);
	float3 halfwayDir = normalize(-sunDirection + viewDir);
	float specularIntensityAdjusted = pow(max(dot(input.worldNormal, -halfwayDir), 0.), specularity) * specularIntensity;
	float3 specularLightColor = float3(1., 1., 1.);
	result.specularLight = specularIntensity * specularLightColor;

	// Shadow
	float2 shadowMapPos;
	shadowMapPos.x = input.lightSpacePosition.x / input.lightSpacePosition.w * .5f + .5f;
	shadowMapPos.y = -input.lightSpacePosition.y / input.lightSpacePosition.w * .5f + .5f;

	float shadow = 0.;
	if (shadowMapPos.x >= 0. && shadowMapPos.x <= 1. && shadowMapPos.y >= 0. && shadowMapPos.y <= 1.)
	{
		float closestDepth = shadowmapTexture.Sample(rawSampler, shadowMapPos).r;
		float currentDepth = input.lightSpacePosition.z / input.lightSpacePosition.w - .003;
		shadow = currentDepth > closestDepth ? 1.0 : 0.0;
	}

	result.diffuseLight *= 1. - shadow;
	result.specularLight *= 1. - shadow;

	return result;
}

float RandomPerPixel(float2 uv, float a = 12.9898, float b = 78.233, float c = 43758.5453123) {
	return frac(sin(dot(uv, float2(a, b))) * c);
}

float3 PostProcess(float3 baseColor, float3 worldPosition)
{
	float3 color = baseColor;

	float fog = postProcessing.w;
	float cameraDistance = length(worldCameraPos - worldPosition);
	float fogPart = cameraDistance * fog * .01;
	float fogFactor = pow(2., -fogPart * fogPart);
	color = lerp(color, fogColor, clamp(1. - fogFactor, 0., 1.));

	float constrast = postProcessing.x;
	float brightness = postProcessing.y;
	color = clamp(constrast * (color - 0.5) + 0.5 + brightness, 0., 1.);
	
	float saturation = postProcessing.z;
	float3 grayscale = dot(color, float3(0.299, 0.587, 0.114));
	color = clamp(lerp(color, grayscale, 1. - saturation), 0., 1.);

	return color;
}
