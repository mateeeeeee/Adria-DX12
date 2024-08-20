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

ConstantBuffer<ComputePrefilteredTextureConstants>ComputePrefilteredTexturePassCB : register(b1);


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
    int2 Position = int2(input.DispatchThreadId.xy);

    float CoCMin = +FLT_MAX;
    float CoCMax = -FLT_MAX;

	float4 ColorSum = float4(0.0, 0.0, 0.0, 0.0);
    for (int SampleIdx = 0; SampleIdx < 4; ++SampleIdx)
    {
        int2 Location = 2 * Position + int2(SampleIdx & 0x01, SampleIdx >> 1);
        float4 Color = colorTexture.Load(int3(Location, 0));
        float CoC    = cocTexture.Load(int3(Location, 0));
        float Weight = ComputeSDRWeight(Color.rgb);
       
        CoCMin = min(CoCMin, CoC);
        CoCMax = max(CoCMax, CoC);
        ColorSum += Color * Weight;
    }

    float ForegroundAlpha = cocDilationTexture.SampleLevel(LinearClampSampler, uv, 0.0);
    float BackgroundAlpha = abs(CoCMax) * float(CoCMax > 0.0);

	foregroundOutputTexture[input.DispatchThreadId.xy] = float4(ColorSum.xyz / max(ColorSum.w, 1.e-5), ForegroundAlpha);
	backgroundOutputTexture[input.DispatchThreadId.xy] = float4(ColorSum.xyz / max(ColorSum.w, 1.e-5), BackgroundAlpha);
}
