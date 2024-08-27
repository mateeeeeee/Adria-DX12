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
	
    const float f = ComputeCoCPassCB.cameraFocalLength / 1000.0f;
    const float K = f * f / (ComputeCoCPassCB.cameraApertureRatio * (ComputeCoCPassCB.cameraFocusDistance - f));
    float CoC = K * (linearDepth - ComputeCoCPassCB.cameraFocusDistance) / max(linearDepth, 1e-4);

	outputTexture[input.DispatchThreadId.xy] = clamp(1000.0f * CoC / (ComputeCoCPassCB.cameraSensorWidth * ComputeCoCPassCB.maxCircleOfConfusion), -1.0, 1.0);
}

struct SeparatedCoCConstants
{
	uint inputIdx;
	uint outputIdx;
};
ConstantBuffer<SeparatedCoCConstants> SeparatedCoCPassCB : register(b1);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ComputeSeparatedCircleOfConfusionCS(CSInput input)
{
    Texture2D<float>   cocTexture  = ResourceDescriptorHeap[SeparatedCoCPassCB.inputIdx];
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[SeparatedCoCPassCB.outputIdx];
	float CoC = cocTexture.Load(int3(input.DispatchThreadId.xy, 0));
	outputTexture[input.DispatchThreadId.xy] = abs(CoC) * float(CoC < 0.0);
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

	int3 lastMipDimension;
	cocMipTexture.GetDimensions(lastMipDimension.x, lastMipDimension.y);

	int2 remappedPosition = int2(2 * input.DispatchThreadId.xy);

	float4 sampledPixels;
	sampledPixels.x = SampleCoC(remappedPosition, int2(0, 0), lastMipDimension);
	sampledPixels.y = SampleCoC(remappedPosition, int2(0, 1), lastMipDimension);
	sampledPixels.z = SampleCoC(remappedPosition, int2(1, 0), lastMipDimension);
	sampledPixels.w = SampleCoC(remappedPosition, int2(1, 1), lastMipDimension);

	float maxCoC = max(max(sampledPixels.x, sampledPixels.y), max(sampledPixels.z, sampledPixels.w));

	bool isWidthOdd = (lastMipDimension.x & 1) != 0;
	bool isHeightOdd = (lastMipDimension.y & 1) != 0;

	if (isWidthOdd)
	{
		sampledPixels.x = SampleCoC(remappedPosition, int2(2, 0), lastMipDimension);
		sampledPixels.y = SampleCoC(remappedPosition, int2(2, 1), lastMipDimension);
		maxCoC = max(maxCoC, max(sampledPixels.x, sampledPixels.y));
	}

	if (isHeightOdd)
	{
		sampledPixels.x = SampleCoC(remappedPosition, int2(0, 2), lastMipDimension);
		sampledPixels.y = SampleCoC(remappedPosition, int2(1, 2), lastMipDimension);
		maxCoC = max(maxCoC, max(sampledPixels.x, sampledPixels.y));
	}

	if (isWidthOdd && isHeightOdd)
	{
		maxCoC = max(maxCoC, SampleCoC(remappedPosition, int2(2, 2), lastMipDimension));
	}

	outputTexture[input.DispatchThreadId.xy] = maxCoC;
}