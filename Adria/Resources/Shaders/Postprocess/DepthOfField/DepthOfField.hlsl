#include "CommonResources.hlsli"
#include "Constants.hlsli"
#include "Tonemapping.hlsli"

#define BLOCK_SIZE 16

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

struct ComputePrefilteredTextureConstants
{
	uint colorIdx;
	uint cocIdx;
	uint cocDilationIdx;
	uint foregroundOutputIdx;
	uint backgroundOutputIdx;
};

ConstantBuffer<ComputePrefilteredTextureConstants> ComputePrefilteredTexturePassCB : register(b1);


float ComputeSDRWeight(float3 Color)
{
    return 1.0 / (1.0 + Luminance(Color));
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ComputePrefilteredTextureCS(CSInput input)
{
    Texture2D<float4>  colorTexture = ResourceDescriptorHeap[ComputePrefilteredTexturePassCB.colorIdx];
    Texture2D<float>   cocTexture   = ResourceDescriptorHeap[ComputePrefilteredTexturePassCB.cocIdx];
    Texture2D<float>   cocDilationTexture = ResourceDescriptorHeap[ComputePrefilteredTexturePassCB.cocDilationIdx];
	RWTexture2D<float4> foregroundOutputTexture = ResourceDescriptorHeap[ComputePrefilteredTexturePassCB.foregroundOutputIdx];
	RWTexture2D<float4> backgroundOutputTexture = ResourceDescriptorHeap[ComputePrefilteredTexturePassCB.backgroundOutputIdx];

	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution / 2);
    int2 position = int2(input.DispatchThreadId.xy);

    float CoCMin = +FLT_MAX;
    float CoCMax = -FLT_MAX;

	float4 colorSum = float4(0.0, 0.0, 0.0, 0.0);
    for (int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
    {
        int2  location = 2 * position + int2(sampleIdx & 0x01, sampleIdx >> 1);
        float4 color = colorTexture.Load(int3(location, 0));
        float CoC    = cocTexture.Load(int3(location, 0));
        float weight = ComputeSDRWeight(color.rgb);
       
        CoCMin = min(CoCMin, CoC);
        CoCMax = max(CoCMax, CoC);
        colorSum += color * weight;
    }

    float foregroundAlpha = cocDilationTexture.SampleLevel(LinearClampSampler, uv, 0.0);
    float backgroundAlpha = abs(CoCMax) * float(CoCMax > 0.0);

	foregroundOutputTexture[input.DispatchThreadId.xy] = float4(colorSum.xyz / max(colorSum.w, 1.e-5), foregroundAlpha);
	backgroundOutputTexture[input.DispatchThreadId.xy] = float4(colorSum.xyz / max(colorSum.w, 1.e-5), backgroundAlpha);
}
