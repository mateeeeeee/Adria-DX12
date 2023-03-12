#include "../Tonemapping.hlsli"
#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

#define BLOCK_SIZE 16

struct TonemapConstants
{   
    float tonemapExposure;
    uint  tonemapOperator;
    uint  hdrIdx;
    uint  exposureIdx;
    int   bloomIdx;
    uint  outputIdx;
    float bloomIntensity;
    float bloomBlendFactor;
};
ConstantBuffer<TonemapConstants> PassCB : register(b1);

struct CS_INPUT
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void Tonemap(CS_INPUT input)
{
    Texture2D<float4> hdrTx = ResourceDescriptorHeap[PassCB.hdrIdx];
    Texture2D<float> exposureTx = ResourceDescriptorHeap[PassCB.exposureIdx];
    RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);
    
    float4 color = hdrTx.Sample(LinearWrapSampler, uv);
	if (PassCB.bloomIdx > 0)
	{
        Texture2D<float4> bloomTx = ResourceDescriptorHeap[PassCB.bloomIdx];
		//float3 lensDirt = tLensDirt.SampleLevel(sLinearClamp, float2(uv.x, 1.0f - uv.y), 0).rgb * cPassData.LensDirtTint;
		float3 bloom = bloomTx.SampleLevel(LinearClampSampler, uv, 0).rgb * PassCB.bloomIntensity;
        color.xyz = lerp(color.xyz, bloom /* + bloom * lensDirt*/, PassCB.bloomBlendFactor);
	}

    float exposure = exposureTx[uint2(0, 0)];
    float4 tone_mapped_color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    switch (PassCB.tonemapOperator)
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
        default:
            tone_mapped_color = float4(color.xyz * exposure * PassCB.tonemapExposure, 1.0f);
    }
    outputTx[input.DispatchThreadId.xy] = tone_mapped_color;
}