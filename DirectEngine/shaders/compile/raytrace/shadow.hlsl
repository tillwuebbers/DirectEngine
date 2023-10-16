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

struct SceneConstantBuffer
{
};

RaytracingAccelerationStructure accelerationStructure : register(t0, space0);
RWTexture2D<float4> renderTarget : register(u0);
ConstantBuffer<SceneConstantBuffer> sceneConstantBuffer : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void MyRaygenShader()
{
    uint2 rayIndexPos = DispatchRaysIndex().xy;
    
    RayDesc myRay =
    {
        float3(rayIndexPos.x - 100., 5.0, rayIndexPos.y - 100.),
        0.1,
        float3(0.0, -1.0, 0.0),
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
    float3 hitColor = float3(1.0, 0.0, 0.0) * (RayTCurrent() / 5.0);
    payload.color = float4(hitColor, 1.0);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0.0f, 0.2f, 0.4f, 1.0f);
}

#endif // RAYTRACING_HLSL