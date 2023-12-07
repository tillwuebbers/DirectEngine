#define PI 3.14159265359
#define MAX_RADIANCE_MIP_LEVEL 4.0

SamplerState rawSampler : register(s0);
SamplerState smoothSampler : register(s1);

cbuffer SceneConstantBuffer : register(b0)
{
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

cbuffer CameraConstantBuffer : register(b4)
{
	float4x4 cameraView;
	float4x4 cameraProjection;
	float4 postProcessing;
	float4 fogData;
	float3 worldCameraPos;
}

Texture2D shadowmapTexture : register(t5);
TextureCube<float4> irradianceMap : register(t6);
TextureCube<float4> reflectanceMap : register(t7);
Texture2D ambientLUT : register(t8);

struct Vertex
{
	float3 position;
	float4 color;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float2 uv;
	float4 boneWeights;
	uint4 boneIndices;
};

struct PSInputDefault
{
	float4 position : SV_POSITION;
	float4 worldPosition : POSITION;
	float4 lightSpacePosition : TEXCOORD1;
	float4 color : COLOR;
	float3 worldNormal : TEXCOORD2;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
	float2 uv : UV;
    float3 lightDirectionTS : TEXCOORD3;
    float3 cameraDirectionTS : TEXCOORD4;
};

float4x4 VSGetVP()
{
	return mul(cameraView, cameraProjection);
}

PSInputDefault VSCalcDefault(float4 position, float3 normal, float3 tangent, float3 bitangent, float2 uv)
{
	PSInputDefault result;

	float4 worldPos = mul(position, worldTransform);
	result.worldPosition = worldPos;

	result.position = mul(worldPos, VSGetVP());

	float4x4 lightVP = mul(lightView, lightProjection);
	result.lightSpacePosition = mul(float4(worldPos.xyz, 1.0), lightVP);

    result.normal = normal;
    result.worldNormal = normalize(mul(float4(normal, 0), worldTransform).xyz);
    result.tangent = normalize(mul(float4(tangent, 0), worldTransform).xyz);
    result.bitangent = normalize(mul(float4(bitangent, 0), worldTransform).xyz);
	result.uv = uv;
	
    float3x3 TBN = float3x3(result.tangent, result.bitangent, result.worldNormal);
    result.lightDirectionTS = mul(TBN, -sunDirection);
    result.cameraDirectionTS = mul(TBN, normalize(worldCameraPos - worldPos.xyz));
	return result;
}

float4 CalcScreenPos(float4 clipPos)
{
	float4 screenPos = clipPos * .5;
	screenPos.xy = float2(screenPos.x, screenPos.y) + screenPos.w;
	screenPos.zw = clipPos.zw;
	return screenPos;
}

struct LightData
{
	float3 ambientLight;
	float3 diffuseLight;
	float3 specularLight;
};

float SampleShadowMap(float2 shadowMapPos, float2 offset, float4 lightSpacePosition, float bias)
{
	float2 samplePos = shadowMapPos + offset / 4096.;

	float shadow = 0.;
	if (samplePos.x >= 0. && samplePos.x <= 1. && samplePos.y >= 0. && samplePos.y <= 1.)
	{
		float closestDepth = shadowmapTexture.Sample(rawSampler, samplePos).r;
		float currentDepth = lightSpacePosition.z / lightSpacePosition.w - bias;
		shadow = currentDepth > closestDepth ? 1.0 : 0.0;
	}
	return shadow;
}

float XPlus(float x)
{
	return x > 0.0 ? 1.0 : 0.0;
}

float G1(float nDotV, float k)
{
    return nDotV / (nDotV * (1.0 - k) + k);
}

float3 FresnelSchlick(float3 F0, float cosTheta, float roughness)
{
	float invRoughness = 1.0 - roughness;
	float3 clampedF0 = max(float3(invRoughness, invRoughness, invRoughness), F0);
    return F0 + (clampedF0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Lighting based on PBR models described by Real-Time Rendering 4th Edition & learnopengl.com
float3 PBR(PSInputDefault input, float3 albedoInput, float3 normalTS, float roughnessInput, float metallicInput)
{
	float3 lightDirectionTS = normalize(input.lightDirectionTS);
	float3 cameraDirectionTS = normalize(input.cameraDirectionTS);
	
    float roughness = roughnessInput * roughnessInput + 0.0001;
    float roughnessSq = roughness * roughness;
	
	// Specularity behavior is different for dielectric and metallic surfaces
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedoInput, metallicInput);
	
	// BRDF for absorbed light
    float3 brdfDiffuse = albedoInput / PI;
	
	// Sum up all light contributions
    float nDotV = max(dot(normalTS, cameraDirectionTS), 0.0);
	float3 lightOut = float3(0.0, 0.0, 0.0);
	
	// TODO: loop over all light sources
    for (int i = 0; i < 1; i++)
    {
        float3 lightColor = float3(1.0, 1.0, 1.0) * 5.0;
		float3 radiance = lightColor; // directional light
        float3 halfWay = normalize(lightDirectionTS + cameraDirectionTS);
		float nDotL = max(dot(normalTS, lightDirectionTS), 0.0);
		float nDotH = max(dot(normalTS, halfWay), 0.0);
		
		// Fresnel-Schlick reflection approximation (F)
        float3 fresnelReflection = FresnelSchlick(F0, nDotL, roughness);
		
		// Microfaced masking and shadowing function (G2)
        float g2 = G1(nDotV, roughness) * G1(nDotL, roughness);
		
		// GGX Microfacet normal distribution function (D)
        float denominator = 1.0 + nDotH * nDotH * (roughnessSq - 1.0);
        float ggx = roughnessSq / (PI * denominator * denominator);
		
		// BRDF for reflected light
        float brdfDenominator = (4.0 * nDotL * nDotV + 0.0001);
        float3 brdfSpecular = (fresnelReflection * ggx * g2) / brdfDenominator;
		
        float3 kD = float3(1.0, 1.0, 1.0) - fresnelReflection;
		kD *= 1.0 - metallicInput;
        
		float inShadow = shadowmapTexture.Load(int3(input.position.xy, 0)).r;
		
        lightOut += (kD * brdfDiffuse + brdfSpecular) * radiance * nDotL * inShadow;
    }
	
	// Environment Light (skybox)
    float3 ambientIrradiance = irradianceMap.Sample(smoothSampler, normalTS).xyz;
	float3 ambientFresnel = FresnelSchlick(F0, nDotV, roughness);
	float3 ambientKD = float3(1.0, 1.0, 1.0) - ambientFresnel;
	float3 ambientDiffuse = ambientKD * albedoInput * ambientIrradiance;
	
    float3 viewDirectionWS = -normalize(worldCameraPos - input.worldPosition.xyz);
    float3 reflectedWS = reflect(viewDirectionWS, input.worldNormal);
    float3 reflectionSample = reflectanceMap.SampleLevel(smoothSampler, reflectedWS, roughness * MAX_RADIANCE_MIP_LEVEL).xyz;
    float2 ambientBRDF = ambientLUT.Sample(smoothSampler, float2(nDotV, roughness)).xy;
    float3 ambientSpecular = reflectionSample * (ambientFresnel * ambientBRDF.x + ambientBRDF.y);
	
    lightOut += ambientKD * ambientDiffuse + ambientSpecular;
	
    return lightOut;
}

LightData PSCalcLightData(PSInputDefault input, float ambientIntensity, float diffuseIntensity, float specularIntensity, float specularity)
{
	LightData result;
	float normalSunAngle = max(dot(input.normal, input.lightDirectionTS), 0.);

	// Ambient
	float3 ambientLightColor = float3(1., 1., 1.);
	result.ambientLight = ambientIntensity * ambientLightColor;

	// Diffuse
    float diffuseIntensityAdjusted = normalSunAngle * diffuseIntensity;
	float3 diffuseLightColor = float3(1., 1., 1.);
	result.diffuseLight = diffuseIntensityAdjusted * diffuseLightColor;

	// Specular
	float3 halfwayDir = normalize(input.lightDirectionTS + input.cameraDirectionTS);
	float specularIntensityAdjusted = pow(max(dot(input.normal, halfwayDir), 0.), specularity) * specularIntensity;
	float3 specularLightColor = float3(1., 1., 1.);
    result.specularLight = specularIntensityAdjusted * specularLightColor;
	
	// Raytraced Shadow
    float inShadow = shadowmapTexture.Load(int3(input.position.xy, 0)).r;
	if (inShadow < 0.99)
    {
        result.diffuseLight = 0;
		result.specularLight = 0;
    }

	return result;
}

float RandomPerPixel(float2 uv, float a = 12.9898, float b = 78.233, float c = 43758.5453123) {
	return frac(sin(dot(uv, float2(a, b))) * c);
}

// ACES Filmic Tone Mapping Curve by Krzysztof Narkowicz
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Blue noise from https://www.shadertoy.com/view/tllcR2
#define hash(p)  frac(sin(dot(p, float2(11.9898, 78.233))) * 43758.5453)
float3 blueNoise(float2 uv)
{
    float v = 0.0;
    for (uint k = 0; k < 9; k++)
        v += hash(uv + float2(k%3-1,k/3-1));
    float noise = 0.9 * (1.125 * hash(uv) - v / 8.0) + 0.5;
    return float3(noise, noise, noise);
}

float3 PostProcess(float3 baseColor, float3 worldPosition)
{
	float3 color = baseColor;

    float fogDensity = fogData.w;
	float cameraDistance = length(worldCameraPos - worldPosition);
    float fogPart = cameraDistance * fogDensity * 0.01;
	float fogFactor = pow(2., -fogPart * fogPart);
    color = lerp(color, fogData.xyz, clamp(1. - fogFactor, 0., 1.));
	
    float exposure = postProcessing.w;
    color = color * exposure;
	
	float constrast = postProcessing.x;
	float brightness = postProcessing.y;
	color = clamp(constrast * (color - 0.5) + 0.5 + brightness, 0., 1.);
	
	float saturation = postProcessing.z;
	float3 grayscale = dot(color, float3(0.299, 0.587, 0.114));
	color = clamp(lerp(color, grayscale, 1. - saturation), 0., 1.);

    float3 tonemappedColor = ACESFilm(color);
	float3 ditherNoise = blueNoise(worldPosition.xy) / 255.;
	float3 ditheredColor = pow(pow(tonemappedColor, 1.0 / 2.2) + ditherNoise, 2.2);
    return ditheredColor;
}
