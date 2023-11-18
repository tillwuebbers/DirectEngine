#include "util/common.hlsl"

StructuredBuffer<Vertex> vertexBufferIn : register(t0);
RWStructuredBuffer<Vertex> vertexBufferOut : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	for (int i = 0; i < 6; i++)
	{
		Vertex v = vertexBufferIn[threadID.x];
		vertexBufferOut[threadID.x + i] = v;
		vertexBufferOut[threadID.x + i].position = v.position + v.normal * (i * .1);
	}
}