#include "../Globals/GlobalsHS.hlsli"




struct VS_CONTROL_POINT_OUTPUT
{
    float4 WorldPos			: POS;
    float2 TexCoord			: TEX;
};

struct HS_CONTROL_POINT_OUTPUT
{
    float3 WorldPos : POS;
    float2 TexCoord : TEX;
};

struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[3]			: SV_TessFactor;
	float InsideTessFactor			: SV_InsideTessFactor;
};

#define NUM_CONTROL_POINTS 3

static const float gMinDist = 100.0;
static const float gMaxDist = 400.0;

static const float gMinTess = 0;
static const float gMaxTess = 2;

int CalcTessFactor(float3 p)
{
    float d = distance(p, frame_cbuf.camera_position.xyz);
    float s = saturate((d - gMinDist) / (gMaxDist - gMinDist));
    return pow(2, lerp(gMaxTess, gMinTess, s));
}

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT Output;

    float3 e0 = 0.5 *   (ip[1].WorldPos.xyz + ip[2].WorldPos.xyz);
    float3 e1 = 0.5 *   (ip[2].WorldPos.xyz + ip[0].WorldPos.xyz);
    float3 e2 = 0.5 *   (ip[0].WorldPos.xyz + ip[1].WorldPos.xyz);
    float3 e3 = 1 / 3 * (ip[0].WorldPos.xyz + ip[1].WorldPos.xyz + ip[2].WorldPos.xyz);
        
    Output.EdgeTessFactor[0] = CalcTessFactor(e0);
    Output.EdgeTessFactor[1] = CalcTessFactor(e1);
    Output.EdgeTessFactor[2] = CalcTessFactor(e2);
    Output.InsideTessFactor  = CalcTessFactor(e3);

	return Output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT main( 
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
	HS_CONTROL_POINT_OUTPUT Output;

    Output.WorldPos = ip[i].WorldPos.xyz;
    Output.TexCoord = ip[i].TexCoord;

	return Output;
}
