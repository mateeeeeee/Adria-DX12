#include "Tonemapping.hlsli"
#include "CommonResources.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16

struct TonemapConstants
{   
    float tonemapExposure;
    uint  tonemapOperatorLUTPacked;
    uint  hdrIdx;
    uint  exposureIdx;
    uint  outputIdx;
	int   bloomIdx;
    uint  lensDirtIdx;
	uint  bloomParamsPacked; //f16 bloomIntensity + f16 bloomBlendFactor;
};
ConstantBuffer<TonemapConstants> PassCB : register(b1);

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void Tonemap(CSInput input)
{
    Texture2D<float4> hdrTx = ResourceDescriptorHeap[PassCB.hdrIdx];
    Texture2D<float> exposureTx = ResourceDescriptorHeap[PassCB.exposureIdx];
    RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
    
    float4 color = hdrTx.Sample(LinearWrapSampler, uv);
	if (PassCB.bloomIdx > 0)
	{
        Texture2D<float4> bloomTx = ResourceDescriptorHeap[PassCB.bloomIdx];
        Texture2D<float4> lensDirtTx = ResourceDescriptorHeap[PassCB.lensDirtIdx];

        float2 bloomParams = UnpackHalf2(PassCB.bloomParamsPacked);
		float3 lensDirt = lensDirtTx.SampleLevel(LinearClampSampler, float2(uv.x, 1.0f - uv.y), 0).rgb;
		float3 bloom = bloomTx.SampleLevel(LinearClampSampler, uv, 0).rgb * bloomParams.x;
        color.xyz = lerp(color.xyz, bloom + bloom * lensDirt, bloomParams.y);
	}

    float exposure = exposureTx[uint2(0, 0)];
    float4 tone_mapped_color = float4(1.0f, 0.0f, 0.0f, 1.0f);

    uint2 toneMapOperatorLUTUnpacked = UnpackTwoUint16FromUint32(PassCB.tonemapOperatorLUTPacked);
    uint tonemapOperator = toneMapOperatorLUTUnpacked.x;
    Texture3D<float3> LUT = ResourceDescriptorHeap[toneMapOperatorLUTUnpacked.y];
    switch (tonemapOperator)
    {
        case 0:
            tone_mapped_color = float4(ReinhardToneMapping(color.xyz * exposure * PassCB.tonemapExposure), 1.0);
            break;
        case 1:
            tone_mapped_color = float4(HableToneMapping(color.xyz * exposure * PassCB.tonemapExposure), 1.0);
            break;
        case 2:
            tone_mapped_color = float4(LinearToneMapping(color.xyz * exposure * PassCB.tonemapExposure), 1.0);
            break;
        case 3:
            tone_mapped_color = float4(TonyMcMapface(LUT, LinearClampSampler, color.xyz * exposure * PassCB.tonemapExposure), 1.0);
            break;
        default:
            tone_mapped_color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    }
	outputTx[input.DispatchThreadId.xy] = tone_mapped_color;
}