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
    uint  outputIdx;
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