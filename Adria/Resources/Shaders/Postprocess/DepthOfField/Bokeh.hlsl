#include "CommonResources.hlsli"
#include "Tonemapping.hlsli"

#ifndef KARIS_INVERSE
#define KARIS_INVERSE 0
#endif

#define BLOCK_SIZE 16

float ComputeHDRWeight(float3 Color)
{
    return 1.0 + Luminance(Color);
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

struct BokehFirstPassConstants
{
	uint colorIdx;
	uint kernelIdx;
	uint cocNearIdx;
	uint cocFarIdx;
	uint output0Idx;
	uint output1Idx;
    uint sampleCount;
    float maxCoC;
};

ConstantBuffer<BokehFirstPassConstants> BokehFirstPassCB : register(b1);

float4 SampleColorCoCNear(float2 Texcoord, float2 Offset)
{
    Texture2D<float4>   cocNearTexture = ResourceDescriptorHeap[BokehFirstPassCB.cocNearIdx];
    return cocNearTexture.SampleLevel(LinearClampSampler, Texcoord + Offset, 0.0);
}

float4 SampleColorCoCFar(float2 Texcoord, float2 Offset)
{
    Texture2D<float4>   cocFarTexture = ResourceDescriptorHeap[BokehFirstPassCB.cocFarIdx];
    return cocFarTexture.SampleLevel(LinearClampSampler, Texcoord + Offset, 0.0);
}

float3 SampleRadiance(float2 Texcoord, float2 Offset)
{
    Texture2D<float4> colorTexture = ResourceDescriptorHeap[BokehFirstPassCB.colorIdx];
    return colorTexture.SampleLevel(LinearClampSampler, Texcoord + Offset, 0.0).rgb;
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BokehFirstPassCS(CSInput input)
{
    Texture2D<float4>   colorTexture                = ResourceDescriptorHeap[BokehFirstPassCB.colorIdx];
    Texture2D<float4>   cocNearTexture              = ResourceDescriptorHeap[BokehFirstPassCB.cocNearIdx];
    Texture2D<float4>   cocFarTexture               = ResourceDescriptorHeap[BokehFirstPassCB.cocFarIdx];
    Texture2D<float2>   kernelTexture               = ResourceDescriptorHeap[BokehFirstPassCB.kernelIdx];
	RWTexture2D<float4> output0Texture              = ResourceDescriptorHeap[BokehFirstPassCB.output0Idx];
	RWTexture2D<float4> output1Texture              = ResourceDescriptorHeap[BokehFirstPassCB.output1Idx];

	float2 texCoord = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution / 2);

    float CoCNear = cocNearTexture.SampleLevel(LinearClampSampler, texCoord, 0).a;
    float CoCFar  = cocFarTexture.SampleLevel(LinearClampSampler, texCoord, 0).a;

    float4 foregroundColor = float4(0.0, 0.0, 0.0, 0.0);
	float4 backgroundColor = float4(0.0, 0.0, 0.0, 0.0);

    float aspectRatio = GetCameraAspectRatio();
    int   sampleCount = BokehFirstPassCB.sampleCount;

    [branch]
    if (CoCNear > 0.0)
    {
        for (int sampleIdx = 0; sampleIdx < sampleCount; sampleIdx++)
        {
            float2 kernelSample = kernelTexture.Load(int3(sampleIdx, 0, 0));

            float2 samplePosition = 0.5f * kernelSample * CoCNear * BokehFirstPassCB.maxCoC;
            float2 sampleTexCoord = float2(samplePosition.x, aspectRatio * samplePosition.y);
            float4 sampledColor = SampleColorCoCNear(texCoord, sampleTexCoord);

#if KARIS_INVERSE
            float weight = ComputeHDRWeight(SampleRadiance(texCoord, sampleTexCoord));
#else
            float weight = 1.0f;
#endif
            foregroundColor += float4(sampledColor.rgb, 1.0f) * weight;
        }
    }

    [branch]
    if (CoCFar > 0.0)
    {
        for (int sampleIdx = 0; sampleIdx < sampleCount; sampleIdx++)
        {
            float2 kernelSample = kernelTexture.Load(int3(sampleIdx, 0, 0));

            float2 samplePosition = 0.5f * kernelSample * CoCFar * BokehFirstPassCB.maxCoC;
            float2 sampleTexCoord = float2(samplePosition.x, aspectRatio * samplePosition.y);
            float4 sampledColor = SampleColorCoCFar(texCoord, sampleTexCoord);

#if KARIS_INVERSE
            float weight = ComputeHDRWeight(SampleRadiance(texCoord, sampleTexCoord));
#else
            float weight = 1.0f;
#endif
            backgroundColor += float4(sampledColor.rgb, 1.0f) * weight * float(sampledColor.a >= CoCFar);
        }
    }

    output0Texture[input.DispatchThreadId.xy] = float4(foregroundColor.xyz * rcp(foregroundColor.w + float(foregroundColor.w == 0.0f)), CoCNear);
    output1Texture[input.DispatchThreadId.xy] = float4(backgroundColor.xyz * rcp(backgroundColor.w + float(backgroundColor.w == 0.0f)), CoCFar);
}



struct BokehSecondPassConstants
{
	uint kernelIdx;
	uint cocNearIdx;
	uint cocFarIdx;
	uint output0Idx;
	uint output1Idx;
    uint sampleCount;
    float maxCoC;
};

ConstantBuffer<BokehSecondPassConstants> BokehSecondPassCB : register(b1);

float4 SampleColorCoCNear2(float2 Texcoord, float2 Offset)
{
    Texture2D<float4>   cocNearTexture = ResourceDescriptorHeap[BokehSecondPassCB.cocNearIdx];
    return cocNearTexture.SampleLevel(LinearClampSampler, Texcoord + Offset, 0.0);
}

float4 SampleColorCoCFar2(float2 Texcoord, float2 Offset)
{
    Texture2D<float4>   cocFarTexture = ResourceDescriptorHeap[BokehSecondPassCB.cocFarIdx];
    return cocFarTexture.SampleLevel(LinearClampSampler, Texcoord + Offset, 0.0);
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BokehSecondPassCS(CSInput input)
{
    Texture2D<float2>   kernelTexture               = ResourceDescriptorHeap[BokehSecondPassCB.kernelIdx];
    Texture2D<float4>   cocNearTexture              = ResourceDescriptorHeap[BokehSecondPassCB.cocNearIdx];
    Texture2D<float4>   cocFarTexture               = ResourceDescriptorHeap[BokehSecondPassCB.cocFarIdx];
    RWTexture2D<float4> output0Texture              = ResourceDescriptorHeap[BokehSecondPassCB.output0Idx];
	RWTexture2D<float4> output1Texture              = ResourceDescriptorHeap[BokehSecondPassCB.output1Idx];

    float2 texCoord = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution / 2);

    float4 foregroundColor  = cocNearTexture.SampleLevel(LinearClampSampler, texCoord, 0);
    float4 backgroundColor  = cocFarTexture.SampleLevel(LinearClampSampler, texCoord, 0);

    float CoCNear = foregroundColor.a;
    float CoCFar  = backgroundColor.a;

    float aspectRatio = GetCameraAspectRatio();
    int   sampleCount = BokehSecondPassCB.sampleCount;
    
    [branch]
    if (CoCNear > 0.0)
    {
        for (int sampleIdx = 0; sampleIdx < sampleCount; sampleIdx++)
        {
            float2 kernelSample = kernelTexture.Load(int3(sampleIdx, 0, 0));

            float2 samplePosition = 0.25f * kernelSample * CoCNear * BokehSecondPassCB.maxCoC;
            float2 sampleTexcoord = float2(samplePosition.x, aspectRatio * samplePosition.y);
            float4 sampledColor = SampleColorCoCNear2(texCoord, sampleTexcoord);
            foregroundColor.xyz = max(sampledColor.rgb, foregroundColor.xyz);
        }
    }

    [branch]
    if (CoCFar > 0.0)
    {
        for (int sampleIdx = 0; sampleIdx < sampleCount; sampleIdx++)
        {
            float2 kernelSample = kernelTexture.Load(int3(sampleIdx, 0, 0));

            float2 samplePosition = 0.25 * kernelSample * CoCFar * BokehSecondPassCB.maxCoC;
            float2 sampleTexCoord = float2(samplePosition.x, aspectRatio * samplePosition.y);
            float4 sampledColor = SampleColorCoCFar2(texCoord, sampleTexCoord);
            backgroundColor.xyz = max(sampledColor.rgb * float(sampledColor.a >= CoCFar), backgroundColor.xyz);
        }
    }

    output0Texture[input.DispatchThreadId.xy] = float4(foregroundColor.xyz, CoCNear);
    output1Texture[input.DispatchThreadId.xy] = float4(backgroundColor.xyz, CoCFar);
}