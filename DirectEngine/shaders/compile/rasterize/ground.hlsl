#include "util/common.hlsl"

PSInputDefault VSMain(float4 position : POSITION, float3 normal : NORMAL, float3 tangent: TANGENT, float3 bitangent: BITANGENT, float2 uv : UV)
{
	PSInputDefault result = VSCalcDefault(position, normal, tangent, bitangent, uv);
	return result;
}

float4 PSMain(PSInputDefault input) : SV_TARGET
{
	float3 color = PBR(input, float3(0.1, 0.1, 0.1), float3(0.0, 0.0, 1.0), 1.0 + 0.0001, 0.0);
    return float4(PostProcess(color, input.worldPosition.rgb), 1.0);
}
