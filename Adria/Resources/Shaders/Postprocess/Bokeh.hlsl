#include "CommonResources.hlsli"


struct Bokeh
{
	float3 Position;
	float2 Size;
	float3 Color;
};

struct BokehConstants
{
	uint bokehIdx;
	uint bokehStackIdx;
};
ConstantBuffer<BokehConstants> BokehPassCB : register(b1);


struct VSToGS
{
	float2 Position : POSITION;
	float2 Size     : SIZE;
	float3 Color    : COLOR;
	float  Depth    : DEPTH;
};

VSToGS BokehVS(in uint VertexID : SV_VertexID)
{
	StructuredBuffer<Bokeh> BokehStack = ResourceDescriptorHeap[BokehPassCB.bokehStackIdx];

	Bokeh bokeh = BokehStack[VertexID];
	VSToGS output;

	output.Position.xy = bokeh.Position.xy;
	output.Position.xy = output.Position.xy * 2.0f - 1.0f;
	output.Position.y *= -1.0f;

	output.Size = bokeh.Size;
	output.Depth = bokeh.Position.z; //depth in view space
	output.Color = bokeh.Color;
	return output;
}

struct GSToPS
{
	float4 PositionCS   : SV_Position;
	float2 TexCoord     : TEXCOORD;
	float3 Color		: COLOR;
	float  Depth		: DEPTH;
};

[maxvertexcount(4)]
void BokehGS(point VSToGS input[1], inout TriangleStream<GSToPS> spriteStream)
{
	const float2 Offsets[4] =
	{
		float2(-1, 1),
		float2(1, 1),
		float2(-1, -1),
		float2(1, -1)
	};

	const float2 TexCoords[4] =
	{
		float2(0, 0),
		float2(1, 0),
		float2(0, 1),
		float2(1, 1)
	};

	GSToPS output;
	[unroll(4)]
	for (int i = 0; i < 4; i++)
	{
		output.PositionCS = float4(input[0].Position.xy, 1.0f, 1.0f);
		output.PositionCS.xy += Offsets[i] * input[0].Size;
		output.TexCoord = TexCoords[i];
		output.Color = input[0].Color;
		output.Depth = input[0].Depth;
		spriteStream.Append(output);
	}
	spriteStream.RestartStrip();
}

float4 BokehPS(GSToPS input) : SV_TARGET
{
	Texture2D bokehTexture = ResourceDescriptorHeap[BokehPassCB.bokehIdx];
	float bokehFactor = bokehTexture.Sample(LinearWrapSampler, input.TexCoord).r;
	return float4(input.Color * bokehFactor, 1.0f);
}