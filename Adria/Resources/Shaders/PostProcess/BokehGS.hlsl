struct GSOutput
{
	float4 pos : SV_POSITION;
};

struct BokehVSOutput
{
    float2 Position : POSITION;
    float2  Size     : SIZE;
    float3 Color    : COLOR;
    float  Depth    : DEPTH;
};

struct BokehGSOutput
{
    float4 PositionCS   : SV_Position;
    float2 TexCoord     : TEXCOORD;
    float3 Color : COLOR;
    float  Depth : DEPTH;
};

static const float2 Offsets[4] =
{
    float2(-1, 1),
	float2(1, 1),
	float2(-1, -1),
	float2(1, -1)
};

static const float2 TexCoords[4] =
{
    float2(0, 0),
	float2(1, 0),
	float2(0, 1),
	float2(1, 1)
};

[maxvertexcount(4)]
void main(point BokehVSOutput input[1], inout TriangleStream<BokehGSOutput> SpriteStream)
{
    BokehGSOutput output;

	[unroll]
    for (int i = 0; i < 4; i++)
    {
        output.PositionCS = float4(input[0].Position.xy, 1.0f, 1.0f);
        output.PositionCS.xy += Offsets[i] * input[0].Size;
        output.TexCoord = TexCoords[i];
        output.Color = input[0].Color;
        output.Depth = input[0].Depth;

        SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}
