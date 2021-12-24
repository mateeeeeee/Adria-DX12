#include "../Globals/GlobalsVS.hlsli"

struct GPUParticlePartA
{
    float4 TintAndAlpha;    
    float Rotation;         
    uint IsSleeping;        
};

StructuredBuffer<GPUParticlePartA> ParticleBufferA : register(t0);
StructuredBuffer<float4> ViewSpacePositions : register(t1);
StructuredBuffer<float2> IndexBuffer : register(t2);

cbuffer ActiveListCountCBuffer : register(b12)
{
    uint NumActiveParticles;
};

struct PS_INPUT
{
    float4 ViewSpaceCentreAndRadius : TEXCOORD0;
    float2 TexCoord                 : TEXCOORD1;
    float3 ViewPos                  : TEXCOORD2;
    float4 Color                    : COLOR0;
    float4 Position                 : SV_POSITION;
};

PS_INPUT main( uint VertexId : SV_VertexID )
{
    PS_INPUT Output = (PS_INPUT) 0;

	uint particleIndex = VertexId / 4;

	uint cornerIndex = VertexId % 4;

    float xOffset = 0;
	
    const float2 offsets[4] =
    {
        float2(-1, 1),
		float2(1, 1),
		float2(-1, -1),
		float2(1, -1),
    };

    uint index = (uint) IndexBuffer[NumActiveParticles - particleIndex - 1].y;
    GPUParticlePartA pa = ParticleBufferA[index];

    float4 ViewSpaceCentreAndRadius = ViewSpacePositions[index];

    float2 offset = offsets[cornerIndex];
    float2 uv = (offset + 1) * float2(0.5, 0.5);

    float radius = ViewSpaceCentreAndRadius.w;
    float3 cameraFacingPos;

    float s, c;
    sincos(pa.Rotation, s, c);
    float2x2 rotation = { float2(c, -s), float2(s, c) };
		
    offset = mul(offset, rotation);

    cameraFacingPos = ViewSpaceCentreAndRadius.xyz;
    cameraFacingPos.xy += radius * offset;

    Output.Position = mul(float4(cameraFacingPos, 1), frame_cbuf.projection);
    Output.TexCoord = uv;
    Output.Color = pa.TintAndAlpha;
    Output.ViewSpaceCentreAndRadius = ViewSpaceCentreAndRadius;
    Output.ViewPos = cameraFacingPos;
	
    return Output;

}