#include "CommonResources.hlsli"

#define LAMBDA 1.2
#define NUM_CONTROL_POINTS 3

static const float MinDest = 100.0;
static const float MaxDest = 400.0;

static const float MinTess = 0;
static const float MaxTess = 3;

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

struct VSInput
{
	float3 Pos : POSITION;
	float2 Uvs : TEX;
};

struct VSToHS
{
	float4 WorldPos : POS;
	float2 TexCoord : TEX;
};

VSToHS OceanVS_LOD(VSInput input)
{
	VSToHS output = (VSToHS)0;
	output.WorldPos = mul(float4(input.Pos, 1.0), PassCB.oceanModelMatrix);
	output.WorldPos /= output.WorldPos.w;
	output.TexCoord = input.Uvs;
	return output;
}

struct HSToDS
{
	float3 WorldPos : POS;
	float2 TexCoord : TEX;
};

struct HSConstantDataOutput
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

HSConstantDataOutput CalcHSPatchConstants(
	InputPatch<VSToHS, NUM_CONTROL_POINTS> inputPatch,
	uint patchId : SV_PrimitiveID)
{
	HSConstantDataOutput Output;

	float3 e0 = 0.5 * (inputPatch[1].WorldPos.xyz + inputPatch[2].WorldPos.xyz);
	float3 e1 = 0.5 * (inputPatch[2].WorldPos.xyz + inputPatch[0].WorldPos.xyz);
	float3 e2 = 0.5 * (inputPatch[0].WorldPos.xyz + inputPatch[1].WorldPos.xyz);
	float3 e3 = 1 / 3 * (inputPatch[0].WorldPos.xyz + inputPatch[1].WorldPos.xyz + inputPatch[2].WorldPos.xyz);

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
HSToDS OceanHS_LOD(
	InputPatch<VSToHS, NUM_CONTROL_POINTS> inputPatch,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	HSToDS Output;
	Output.WorldPos = inputPatch[i].WorldPos.xyz;
	Output.TexCoord = inputPatch[i].TexCoord;
	return Output;
}

struct DSToPS
{
	float4 Position		: SV_POSITION;
	float4 WorldPos		: POS;
	float2 TexCoord		: TEX;
};

[domain("tri")]
DSToPS OceanDS_LOD(
	HSConstantDataOutput input,
	float3 domain : SV_DomainLocation,
	const OutputPatch<HSToDS, NUM_CONTROL_POINTS> patch)
{
	DSToPS output;
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

