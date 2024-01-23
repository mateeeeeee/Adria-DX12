#include "CommonResources.hlsli"

struct Constants
{
	uint   rainDataIdx;
	uint   rainIdx;
};
ConstantBuffer<Constants> PassCB : register(b1);

struct RainData
{
	float3 Pos;
	float3 Vel;
	float State;
};

struct VSToPS
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEX;
	float Clip		: SV_ClipDistance0;
};

static const float2 PositionOffsets[6] =
{
 	float2( 1.0, -1.0 ),
 	float2( 1.0, 0.0 ),
 	float2( -1.0, -1.0 ),
 	float2( -1.0, -1.0 ),
	float2( 1.0, 0.0 ),
	float2( -1.0, 0.0 ),
};
    
static const float2 UVs[6] = 
{ 
	float2(1.0, 0.0),
	float2(1.0, 1.0),
	float2(0.0, 0.0),

	float2(0.0, 0.0),
	float2(1.0, 1.0),
	float2(0.0, 1.0),
};

VSToPS RainVS(uint VertexID : SV_VERTEXID)
{
	VSToPS output = (VSToPS)0;

	StructuredBuffer<RainData> rainDataBuffer = ResourceDescriptorHeap[PassCB.rainDataIdx];
	RainData rainDrop = RainData[VertexID / 6];

	float3x3 viewRotation = (float3x3)FrameCB.view;
    float3 viewDir = -viewRotation[2];

    float3 pos = rainDrop.Pos;
    float3 rainDir = normalize(rainDrop.Vel);
	float3 rainRight = normalize(cross(viewDir, rainDir));

	float2 offsets = PositionOffsets[VertexID % 6];
	pos += rainRight * offsets.x * Scale * 0.025;
	pos += rainDir * offsets.y * Scale;

	output.Pos = mul(float4(pos, 1.0), FrameCB.viewProjection);
	output.Tex = UVs[VertexID % 6];
	output.Clip = rainDrop.State;
	return output;
}

float4 RainPS(VSToPS input) : SV_TARGET
{
	Texture2D rainTx = ResourceDescriptorHeap[PassCB.rainIdx];
	float rainAlpha = rainTx.Sample(LinearWrapSampler, input.TexCoord).r;
	return float4(FrameCB.ambientColor.rgb, rainAlpha);
}
