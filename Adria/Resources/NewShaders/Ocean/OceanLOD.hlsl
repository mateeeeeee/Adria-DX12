
#include "../CommonResources.hlsli"

#define LAMBDA 1.2
#define NUM_CONTROL_POINTS 3

static const float MinDest = 100.0;
static const float MaxDest = 400.0;

static const float MinTess = 0;
static const float MaxTess = 2;

struct OceanIndices
{
	uint displacementIdx;
	uint normalIdx;
	uint skyIdx;
	uint foamIdx;
};
ConstantBuffer<OceanIndices> PassCB2 : register(b1);

struct OceanConstants
{
	row_major matrix oceanModelMatrix;
	float3 oceanColor;
};
ConstantBuffer<OceanConstants> PassCB : register(b2);

struct VS_INPUT
{
	float3 Pos : POSITION;
	float2 Uvs : TEX;
};

struct VS_CONTROL_POINT_OUTPUT
{
	float4 WorldPos : POS;
	float2 TexCoord : TEX;
};

VS_CONTROL_POINT_OUTPUT OceanLOD_VS(VS_INPUT vin)
{
	VS_CONTROL_POINT_OUTPUT vout;
	vout.WorldPos = mul(float4(vin.Pos, 1.0), PassCB.oceanModelMatrix);
	vout.WorldPos /= vout.WorldPos.w;
	vout.TexCoord = vin.Uvs;
	return vout;
}

struct HS_CONTROL_POINT_OUTPUT
{
	float3 WorldPos : POS;
	float2 TexCoord : TEX;
};

struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[3]	: SV_TessFactor;
	float InsideTessFactor  : SV_InsideTessFactor;
};

int CalcTessFactor(float3 p)
{
	float d = distance(p, FrameCB.cameraPosition.xyz);
	float s = saturate((d - MinDest) / (MaxDest - MinDest));
	return pow(2, lerp(MaxTess, MinTess, s));
}

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT Output;

	float3 e0 = 0.5 * (ip[1].WorldPos.xyz + ip[2].WorldPos.xyz);
	float3 e1 = 0.5 * (ip[2].WorldPos.xyz + ip[0].WorldPos.xyz);
	float3 e2 = 0.5 * (ip[0].WorldPos.xyz + ip[1].WorldPos.xyz);
	float3 e3 = 1 / 3 * (ip[0].WorldPos.xyz + ip[1].WorldPos.xyz + ip[2].WorldPos.xyz);

	Output.EdgeTessFactor[0] = CalcTessFactor(e0);
	Output.EdgeTessFactor[1] = CalcTessFactor(e1);
	Output.EdgeTessFactor[2] = CalcTessFactor(e2);
	Output.InsideTessFactor = CalcTessFactor(e3);

	return Output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT OceanLOD_HS(
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONTROL_POINT_OUTPUT Output;
	Output.WorldPos = ip[i].WorldPos.xyz;
	Output.TexCoord = ip[i].TexCoord;
	return Output;
}

struct DS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float4 WorldPos		: POS;
	float2 TexCoord		: TEX;
};

[domain("tri")]
DS_OUTPUT OceanLOD_DS(
	HS_CONSTANT_DATA_OUTPUT input,
	float3 domain : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
	DS_OUTPUT output;
	output.TexCoord =
		domain.x * patch[0].TexCoord +
		domain.y * patch[1].TexCoord +
		domain.z * patch[2].TexCoord;

	float3 WorldPos = domain.x * patch[0].WorldPos +
		domain.y * patch[1].WorldPos +
		domain.z * patch[2].WorldPos;

	Texture2D displacementTx = ResourceDescriptorHeap[PassCB2.displacementIdx];
	float3 displacement = displacementTx.SampleLevel(LinearWrapSampler, output.TexCoord, 0.0f).xyz;
	WorldPos += displacement * LAMBDA;

	output.WorldPos = float4(WorldPos, 1.0f);
	output.Position = mul(output.WorldPos, FrameCB.viewProjection);
	return output;
}

