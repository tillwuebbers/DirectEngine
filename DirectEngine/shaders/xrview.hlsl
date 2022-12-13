#include "common.hlsl"

Texture2D viewTexture : register(t7);

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : UV;
};

PSInput VSMain(uint id : SV_VertexID)
{
	PSInput result;
	float2 pos = float2(0., 0.);
	float2 uv = float2(0., 0.);
	switch (id)
	{
	case 0:
		pos.x = -1.;
		pos.y = -1.;
		uv.x = 0.;
		uv.y = 0.;
		break;
	case 1:
		pos.x = -1.;
		pos.y = 3.;
		uv.x = 0.;
		uv.y = 2.;
		break;
	case 2:
		pos.x = 3.;
		pos.y = -1.;
		uv.x = 2.;
		uv.y = 0.;
		break;
	}

	result.position = float4(pos, 0., 1.);
	result.uv = uv;
	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return viewTexture.Sample(smoothSampler, input.uv);
}
