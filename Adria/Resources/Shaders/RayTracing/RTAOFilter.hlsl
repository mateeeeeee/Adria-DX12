#include "CommonResources.hlsli"

#define BLOCK_SIZE 32

struct RTAOFilterIndices
{
	uint  depthIdx;
	uint  inputIdx;
	uint  outputIdx;
};
ConstantBuffer<RTAOFilterIndices> RTAOFilterPassCB : register(b1);

struct RTAOFilterConstants
{
	float filterWidth;
	float filterHeight;
	float filterDistanceSigma;
	float filterDepthSigma;
	float filterDistKernel0;
	float filterDistKernel1;
	float filterDistKernel2;
	float filterDistKernel3;
	float filterDistKernel4;
	float filterDistKernel5;
};
ConstantBuffer<RTAOFilterConstants> RTAOFilterPassCB2 : register(b2);



static const int FILTER_RADIUS = 5;
static const int PADDED_PIXEL_WIDTH = BLOCK_SIZE + FILTER_RADIUS * 2;
static const int PADDED_PIXEL_COUNT = PADDED_PIXEL_WIDTH * PADDED_PIXEL_WIDTH;
groupshared float2 DistanceAndAO[PADDED_PIXEL_WIDTH][PADDED_PIXEL_WIDTH];

float FilterAO(int2 paddedPixelPos)
{
	if (RTAOFilterPassCB2.filterDistanceSigma <= 0.f || RTAOFilterPassCB2.filterDepthSigma <= 0.f)
	{
		return DistanceAndAO[paddedPixelPos.x][paddedPixelPos.y].y;
	}

	float distanceKernel[FILTER_RADIUS + 1];
	distanceKernel[0] = RTAOFilterPassCB2.filterDistKernel0;
	distanceKernel[1] = RTAOFilterPassCB2.filterDistKernel1;
	distanceKernel[2] = RTAOFilterPassCB2.filterDistKernel2;
	distanceKernel[3] = RTAOFilterPassCB2.filterDistKernel3;
	distanceKernel[4] = RTAOFilterPassCB2.filterDistKernel4;
	distanceKernel[5] = RTAOFilterPassCB2.filterDistKernel5;

	float totalWeight = 0.f;
	float sum = 0.f;
	float centerDepth = DistanceAndAO[paddedPixelPos.x][paddedPixelPos.y].x;

	for (int y = -FILTER_RADIUS; y <= FILTER_RADIUS; ++y)
	{
		for (int x = -FILTER_RADIUS; x <= FILTER_RADIUS; ++x)
		{
			float weight = distanceKernel[abs(x)] * distanceKernel[abs(y)];
			float depth = DistanceAndAO[paddedPixelPos.x + x][paddedPixelPos.y + y].x;
			float depthDifference = depth - centerDepth;
			float depthSigma = RTAOFilterPassCB2.filterDepthSigma;

			weight *= exp(-(depthDifference * depthDifference) / (2.f * depthSigma * depthSigma));
			sum += DistanceAndAO[paddedPixelPos.x + x][paddedPixelPos.y + y].y * weight;
			totalWeight += weight;
		}
	}
	return sum / totalWeight;
}


struct CSInput
{
	uint3 GroupID : SV_GroupID;
	uint3 GroupThreadID : SV_GroupThreadID;
	uint3 DispatchThreadID : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void RTAOFilterCS(CSInput input)
{
	Texture2D<float> rtaoTexture = ResourceDescriptorHeap[RTAOFilterPassCB.inputIdx];
	Texture2D<float> depthTexture = ResourceDescriptorHeap[RTAOFilterPassCB.depthIdx];
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[RTAOFilterPassCB.outputIdx];

	// Load hit distance and ambient occlusion for this pixel and share it with the thread group
	int2 pixelBase = int2(input.GroupID.xy) * int2(BLOCK_SIZE, BLOCK_SIZE) - int2(FILTER_RADIUS, FILTER_RADIUS);
	int  pixelIndex = int(input.GroupIndex);

	static const int unpaddedPixelCount = BLOCK_SIZE * BLOCK_SIZE;
	static const int loopCount = (PADDED_PIXEL_COUNT % unpaddedPixelCount) ? (1 + PADDED_PIXEL_COUNT / unpaddedPixelCount) : (PADDED_PIXEL_COUNT / unpaddedPixelCount);

	// Cooperatively load the data into shared memory
	for (int i = 0; i < loopCount; ++i)
	{
		int2 paddedPixel = int2(pixelIndex % PADDED_PIXEL_WIDTH, pixelIndex / PADDED_PIXEL_WIDTH);
		if (paddedPixel.x < PADDED_PIXEL_WIDTH && paddedPixel.y < PADDED_PIXEL_WIDTH)
		{
			int2 srcPixel = paddedPixel + pixelBase;
			if (srcPixel.x < 0 || srcPixel.y < 0 || srcPixel.x >= RTAOFilterPassCB2.filterWidth || srcPixel.y >= RTAOFilterPassCB2.filterHeight)
			{
				DistanceAndAO[paddedPixel.x][paddedPixel.y] = float2(0.0f, 0.0f);
			}
			else
			{
				float depth = depthTexture.Load(int3(srcPixel, 0));
				float2 texCoords = (srcPixel + 0.5f) / float2(RTAOFilterPassCB2.filterWidth, RTAOFilterPassCB2.filterHeight);
				float3 worldPosition = GetWorldPosition(texCoords, depth);
				float distance = length(worldPosition - FrameCB.cameraPosition.xyz);
				float occlusion = rtaoTexture.Load(int3(srcPixel, 0));
				DistanceAndAO[paddedPixel.x][paddedPixel.y] = float2(distance, occlusion);
			}
		}

		// Move to the next pixel
		pixelIndex += (BLOCK_SIZE * BLOCK_SIZE);
	}

	// Wait for the thread group to sync
	GroupMemoryBarrierWithGroupSync();

	// Filter using group shared memory
	{
		int2 pixelIndex = int2(input.GroupThreadID.xy) + int2(FILTER_RADIUS, FILTER_RADIUS);
		outputTexture[input.DispatchThreadID.xy] = FilterAO(pixelIndex);
	}
}