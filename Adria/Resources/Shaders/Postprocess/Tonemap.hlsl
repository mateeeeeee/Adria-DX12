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
ConstantBuffer<TonemapConstants> TonemapPassCB : register(b1);

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void TonemapCS(CSInput input)
{
    Texture2D<float4> hdrTexture = ResourceDescriptorHeap[TonemapPassCB.hdrIdx];
    Texture2D<float> exposureTexture = ResourceDescriptorHeap[TonemapPassCB.exposureIdx];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[TonemapPassCB.outputIdx];

    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
    
    float4 color = hdrTexture.Sample(LinearWrapSampler, uv);
	if (TonemapPassCB.bloomIdx > 0)
	{
        Texture2D<float4> bloomTx = ResourceDescriptorHeap[TonemapPassCB.bloomIdx];
        Texture2D<float4> lensDirtTx = ResourceDescriptorHeap[TonemapPassCB.lensDirtIdx];

        float2 bloomParams = UnpackHalf2(TonemapPassCB.bloomParamsPacked);
		float3 lensDirt = lensDirtTx.SampleLevel(LinearClampSampler, float2(uv.x, 1.0f - uv.y), 0).rgb;
		float3 bloom = bloomTx.SampleLevel(LinearClampSampler, uv, 0).rgb * bloomParams.x;
        color.xyz = lerp(color.xyz, bloom + bloom * lensDirt, bloomParams.y);
	}
    color.xyz *= TonemapPassCB.tonemapExposure;
    float exposure = exposureTexture[uint2(0, 0)];
    color.xyz *= exposure;

    float4 toneMappedColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
    uint2 toneMapOperatorLUTUnpacked = UnpackTwoUint16FromUint32(TonemapPassCB.tonemapOperatorLUTPacked);
    uint tonemapOperator = toneMapOperatorLUTUnpacked.x;
    Texture3D<float3> LUT = ResourceDescriptorHeap[toneMapOperatorLUTUnpacked.y];

    switch (tonemapOperator)
    {
        case Tonemap_None:
            toneMappedColor = float4(color.xyz, 1.0f);
            break;
        case Tonemap_Reinhard:
            toneMappedColor = float4(ReinhardToneMapping(color.xyz), 1.0);
            break;
        case Tonemap_Hable:
            toneMappedColor = float4(HableToneMapping(color.xyz), 1.0);
            break;
        case Tonemap_Linear:
            toneMappedColor = float4(LinearToneMapping(color.xyz), 1.0);
            break;
        case Tonemap_TonyMcMapface:
            toneMappedColor = float4(TonyMcMapface(LUT, LinearClampSampler, color.xyz), 1.0);
            break;
        case Tonemap_AgX:
            toneMappedColor = float4(AgXToneMapping(color.xyz), 1.0);

            break;
    }
	outputTexture[input.DispatchThreadId.xy] = toneMappedColor;
}