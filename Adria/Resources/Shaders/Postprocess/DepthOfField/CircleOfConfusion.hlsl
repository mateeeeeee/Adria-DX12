#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

struct ComputeCoCConstants
{
	uint depthIdx;
	uint outputIdx;
	float cameraFocalLength;
	float cameraFocusDistance;
	float cameraApertureRatio;
	float cameraSensorWidth;
	float maxCircleOfConfusion;
};

ConstantBuffer<ComputeCoCConstants> ComputeCoCPassCB : register(b1);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ComputeCircleOfConfusionCS(CSInput input)
{
    Texture2D<float>   depthTexture = ResourceDescriptorHeap[ComputeCoCPassCB.depthIdx];
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[ComputeCoCPassCB.outputIdx];
	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
	
    float depth = depthTexture.Sample(LinearBorderSampler, uv);
	float linearDepth = LinearizeDepth(depth);
	
    const float f = ComputeCoCPassCB.cameraFocalLength;
    const float K = f * f / (ComputeCoCPassCB.cameraApertureRatio * (ComputeCoCPassCB.cameraFocusDistance - f));
    float CoC = K * (linearDepth - ComputeCoCPassCB.cameraFocusDistance) / max(linearDepth, 1e-4);

	outputTexture[input.DispatchThreadId.xy] = clamp(1000.0 * CoC / (ComputeCoCPassCB.cameraSensorWidth * ComputeCoCPassCB.maxCircleOfConfusion), -1.0, 1.0);
}

struct DownsampleCoCConstants
{
	uint inputIdx;
	uint outputIdx;
};

ConstantBuffer<DownsampleCoCConstants> DownsampleCoCPassCB : register(b1);

int2 ClampScreenCoord(int2 PixelCoord, int2 Dimension)
{
	return clamp(PixelCoord, int2(0, 0), Dimension - int2(1, 1));
}
float SampleCoC(int2 Location, int2 Offset, int3 Dimension)
{
	Texture2D<float> cocMipTexture = ResourceDescriptorHeap[DownsampleCoCPassCB.inputIdx];
	int2 Position = clamp(Location + Offset, int2(0, 0), Dimension.xy - int2(1, 1)); 
	return cocMipTexture.Load(int3(Position, 0));
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void DownsampleCircleOfConfusionCS(CSInput input)
{
	Texture2D<float> cocMipTexture = ResourceDescriptorHeap[DownsampleCoCPassCB.inputIdx];
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[DownsampleCoCPassCB.outputIdx];

	int3 LastMipDimension;
	cocMipTexture.GetDimensions(LastMipDimension.x, LastMipDimension.y);

	int2 RemappedPosition = int2(2 * input.DispatchThreadId.xy);

	float4 SampledPixels;
	SampledPixels.x = SampleCoC(RemappedPosition, int2(0, 0), LastMipDimension);
	SampledPixels.y = SampleCoC(RemappedPosition, int2(0, 1), LastMipDimension);
	SampledPixels.z = SampleCoC(RemappedPosition, int2(1, 0), LastMipDimension);
	SampledPixels.w = SampleCoC(RemappedPosition, int2(1, 1), LastMipDimension);

	float MaxCoC = max(max(SampledPixels.x, SampledPixels.y), max(SampledPixels.z, SampledPixels.w));

	bool IsWidthOdd = (LastMipDimension.x & 1) != 0;
	bool IsHeightOdd = (LastMipDimension.y & 1) != 0;

	if (IsWidthOdd)
	{
		SampledPixels.x = SampleCoC(RemappedPosition, int2(2, 0), LastMipDimension);
		SampledPixels.y = SampleCoC(RemappedPosition, int2(2, 1), LastMipDimension);
		MaxCoC = max(MaxCoC, max(SampledPixels.x, SampledPixels.y));
	}

	if (IsHeightOdd)
	{
		SampledPixels.x = SampleCoC(RemappedPosition, int2(0, 2), LastMipDimension);
		SampledPixels.y = SampleCoC(RemappedPosition, int2(1, 2), LastMipDimension);
		MaxCoC = max(MaxCoC, max(SampledPixels.x, SampledPixels.y));
	}

	if (IsWidthOdd && IsHeightOdd)
	{
		MaxCoC = max(MaxCoC, SampleCoC(RemappedPosition, int2(2, 2), LastMipDimension));
	}

	outputTexture[input.DispatchThreadId.xy] = MaxCoC;
}