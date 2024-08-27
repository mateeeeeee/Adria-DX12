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


float ComputeSDRWeight(float3 Color)
{
    return 1.0 / (1.0 + Luminance(Color));
}

struct ComputePrefilteredTextureConstants
{
	uint colorIdx;
	uint cocIdx;
	uint cocDilationIdx;
	uint foregroundOutputIdx;
	uint backgroundOutputIdx;
};

ConstantBuffer<ComputePrefilteredTextureConstants> ComputePrefilteredTexturePassCB : register(b1);


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
        colorSum += float4(color.rgb, 1.0) * weight; 
    }

    float foregroundAlpha = cocDilationTexture.SampleLevel(LinearClampSampler, uv, 0.0);
    float backgroundAlpha = abs(CoCMax) * float(CoCMax > 0.0);

	foregroundOutputTexture[input.DispatchThreadId.xy] = float4(colorSum.xyz / max(colorSum.w, 1.e-5), foregroundAlpha);
	backgroundOutputTexture[input.DispatchThreadId.xy] = float4(colorSum.xyz / max(colorSum.w, 1.e-5), backgroundAlpha);
}


struct ComputePostfilteredTextureConstants
{
	uint nearCoCIdx;
	uint farCoCIdx;
	uint foregroundOutputIdx;
	uint backgroundOutputIdx;
};

ConstantBuffer<ComputePostfilteredTextureConstants> ComputePostfilteredTexturePassCB : register(b1);


[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ComputePostfilteredTextureCS(CSInput input)
{
    Texture2D<float4>   nearCoCTexture   = ResourceDescriptorHeap[ComputePostfilteredTexturePassCB.nearCoCIdx];
    Texture2D<float4>   farCoCTexture    = ResourceDescriptorHeap[ComputePostfilteredTexturePassCB.farCoCIdx];
	RWTexture2D<float4> foregroundOutputTexture = ResourceDescriptorHeap[ComputePostfilteredTexturePassCB.foregroundOutputIdx];
	RWTexture2D<float4> backgroundOutputTexture = ResourceDescriptorHeap[ComputePostfilteredTexturePassCB.backgroundOutputIdx];

	float2 TextureResolution;
    nearCoCTexture.GetDimensions(TextureResolution.x, TextureResolution.y);

    float2 texelSize = rcp(TextureResolution);
	float2 texCoord = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution / 2);
    
    float4 A = nearCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(-0.5f, -0.5f), 0);
    float4 B = nearCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(-0.5f, +0.5f), 0);
    float4 C = nearCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(+0.5f, -0.5f), 0);
    float4 D = nearCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(+0.5f, +0.5f), 0);

    float4 E = farCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(-0.5f, -0.5f), 0);
    float4 F = farCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(-0.5f, +0.5f), 0);
    float4 G = farCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(+0.5f, -0.5f), 0);
    float4 H = farCoCTexture.SampleLevel(LinearClampSampler, texCoord + texelSize * float2(+0.5f, +0.5f), 0);

    foregroundOutputTexture[input.DispatchThreadId.xy] =  0.25f * (A + B + C + D);
	backgroundOutputTexture[input.DispatchThreadId.xy] =  0.25f * (E + F + G + H);
}

struct CombineConstants
{
	uint colorIdx;
	uint nearCoCIdx;
	uint farCoCIdx;
	uint outputIdx;
    float alphaInterpolation;
};

ConstantBuffer<CombineConstants> CombinePassCB : register(b1);


[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CombineCS(CSInput input)
{
    Texture2D<float4>   colorTexture   = ResourceDescriptorHeap[CombinePassCB.colorIdx];
    Texture2D<float4>   nearCoCTexture = ResourceDescriptorHeap[CombinePassCB.nearCoCIdx];
    Texture2D<float4>   farCoCTexture  = ResourceDescriptorHeap[CombinePassCB.farCoCIdx];
	RWTexture2D<float4> outputTexture  = ResourceDescriptorHeap[CombinePassCB.outputIdx];    

	float2 texCoord = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
    float4 sourceFullRes = colorTexture.Load(int3(input.DispatchThreadId.xy, 0));

    float4 nearDoF = nearCoCTexture.SampleLevel(LinearClampSampler, texCoord, 0);
    float4 farDoF  = farCoCTexture.SampleLevel(LinearClampSampler, texCoord, 0);

    float4 result = sourceFullRes;
    result.rgb = lerp(result.rgb, farDoF.rgb, smoothstep(0.1, 1.0, farDoF.a));
    result.rgb = lerp(result.rgb, nearDoF.rgb, smoothstep(0.1, 1.0, nearDoF.a));
    outputTexture[input.DispatchThreadId.xy] = lerp(sourceFullRes, result, CombinePassCB.alphaInterpolation);
}