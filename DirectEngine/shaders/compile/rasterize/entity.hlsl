#include "util/common.hlsl"

#ifdef DIFFUSE_TEXTURE
Texture2D diffuseTexture : register(t0, space1);
#endif

#ifdef NORMAL_TEXTURE
Texture2D normalTexture : register(t1, space1);
#endif

#ifdef METAL_ROUGHNESS_TEXTURE
Texture2D metalRoughnessTexture : register(t2, space1);
#endif

PSInputDefault VSMain(float4 position : POSITION, float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, float2 uv : UV, float4 boneWeights : BONE_WEIGHTS, uint4 boneIndices : BONE_INDICES)
{
    #ifdef SKINNED_MESH
    float4 vertexPos = boneWeights.x * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.x], jointTransforms[boneIndices.x])).xyz, 1.0);
	vertexPos       += boneWeights.y * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.y], jointTransforms[boneIndices.y])).xyz, 1.0);
	vertexPos       += boneWeights.z * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.z], jointTransforms[boneIndices.z])).xyz, 1.0);
	vertexPos       += boneWeights.w * float4(mul(float4(position.xyz, 1.0), mul(inverseJointBinds[boneIndices.w], jointTransforms[boneIndices.w])).xyz, 1.0);
	vertexPos.w = 1.0;
    #else
    float4 vertexPos = position;
    #endif
    
    PSInputDefault result = VSCalcDefault(vertexPos, normal, tangent, bitangent, uv);
    return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
    #ifdef DIFFUSE_TEXTURE
    float4 albedoSample = diffuseTexture.Sample(smoothSampler, input.uv);
    #else
    float4 albedoSample = diffuseColor;
    #endif
    
    #ifdef ALPHA_CLIP
    if (albedoSample.w < 0.5)
        discard;
    #endif
    
    #ifdef NORMAL_TEXTURE
    float2 normalSample = normalTexture.Sample(smoothSampler, input.uv).xy * 2.0 - 1.0;
    float normalZ = sqrt(saturate(1.0 - dot(normalSample, normalSample)));
    float3 normalTS = float3(normalSample, normalZ);
    #else
    float3 normalTS = float3(0.0, 0.0, 1.0);
    #endif
    
    #ifdef METAL_ROUGHNESS_TEXTURE
    float4 metalRoughnessSample = metalRoughnessTexture.Sample(smoothSampler, input.uv);
    #else
    float4 metalRoughnessSample = metalRoughness;
    #endif
    
    float3 appliedColor = PBR(input, albedoSample.xyz, normalTS, metalRoughnessSample.y, metalRoughnessSample.z);
    return float4(PostProcess(appliedColor, input.worldPosition.rgb), 1.0);
}
