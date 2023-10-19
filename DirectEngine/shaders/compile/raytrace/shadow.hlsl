//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL

// Subobjects definitions at library scope. 
GlobalRootSignature MyGlobalRootSignature =
{
    "DescriptorTable( UAV( u0 ) ),"                        // Output texture
    "SRV( t0 ),"                                           // Acceleration structure
    "DescriptorTable( SRV( t1 ) ),"                        // GBuffer (normals)
    "DescriptorTable( SRV( t2 ) ),"                        // GBuffer (world positions)
    "DescriptorTable( CBV( b0 ) )"                         // Scene constant buffer
};

LocalRootSignature MyLocalRootSignature =
{
    "RootConstants( num32BitConstants = 4, b1 )"           // Cube constants        
};

TriangleHitGroup MyHitGroup =
{
    "", // AnyHit
    "MyClosestHitShader", // ClosestHit
};

SubobjectToExportsAssociation MyLocalRootSignatureAssociation =
{
    "MyLocalRootSignature", // subobject name
    "MyHitGroup"            // export association 
};

RaytracingShaderConfig MyShaderConfig =
{
    16, // max payload size
    8 // max attribute size
};

RaytracingPipelineConfig MyPipelineConfig =
{
    1 // max trace recursion depth
};

struct LightConstantBuffer
{
    float4x4 lightView;
    float4x4 lightProjection;
    float4 sunDirection;
};

RWTexture2D<float4> renderTarget : register(u0);
RaytracingAccelerationStructure accelerationStructure : register(t0, space0);
Texture2D<float4> gBufferNormal : register(t1);
Texture2D<float4> gBufferWorldPosition : register(t2);
ConstantBuffer<LightConstantBuffer> lightConstantBuffer : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void MyRaygenShader()
{
    uint2 rayIndexPos = DispatchRaysIndex().xy;
    uint2 outputDimensions = DispatchRaysDimensions().xy;
    float2 uv = float2(rayIndexPos) / float2(outputDimensions);
    
    int3 samplePos = int3(rayIndexPos, 0);
    float4 normalAndDepth = gBufferNormal.Load(samplePos);
    float4 worldPosition = gBufferWorldPosition.Load(samplePos);
    float3 lightDirection = -lightConstantBuffer.sunDirection.xyz;
    
    if (dot(normalAndDepth.xyz, lightDirection) < .2 || worldPosition.a < 0.01)
    {
        renderTarget[rayIndexPos.xy] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    
    RayDesc myRay =
    {
        worldPosition.xyz,
        0.01,
        lightDirection,
        1000.0
    };

    RayPayload payload = { float4(0.0, 0.0, 0.0, 0.0) }; // init payload

    TraceRay(
        accelerationStructure,
        0x0,
        0x1,
        0,
        0,
        0,
        myRay,
        payload);

    renderTarget[rayIndexPos.xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    payload.color = float4(0.0, 0.0, 0.0, 1.0);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
}

#endif // RAYTRACING_HLSL