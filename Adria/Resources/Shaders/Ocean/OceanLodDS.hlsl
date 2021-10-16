#include "../Globals/GlobalsHS.hlsli"

struct DS_OUTPUT
{
	float4 vPosition	: SV_POSITION;
    float4 WorldPos		: POS;
    float2 TexCoord		: TEX;
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


Texture2D DisplacementMap : register(t0);
SamplerState linear_wrap_sampler : register(s0);

#define NUM_CONTROL_POINTS 3
#define LAMBDA 1.2

[domain("tri")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,
	float3 domain : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
	DS_OUTPUT Output;
	
    Output.TexCoord =
                        domain.x * patch[0].TexCoord +
                        domain.y * patch[1].TexCoord +
                        domain.z * patch[2].TexCoord;
    
    float3 WorldPos =   domain.x * patch[0].WorldPos +
                        domain.y * patch[1].WorldPos +
                        domain.z * patch[2].WorldPos;
    

    float3 D_x = DisplacementMap.SampleLevel(linear_wrap_sampler, Output.TexCoord, 0.0f).xyz;
    
    WorldPos += D_x * LAMBDA;

    Output.WorldPos = float4(WorldPos, 1.0f);

    Output.vPosition = mul(Output.WorldPos, frame_cbuf.viewprojection);
	
	return Output;
}
