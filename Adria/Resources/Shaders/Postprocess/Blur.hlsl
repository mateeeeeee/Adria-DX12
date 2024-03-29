#include "CommonResources.hlsli"

#define BLOCK_SIZE 1024

struct BlurConstants
{
	uint inputIdx;
	uint outputIdx;
};
ConstantBuffer<BlurConstants> PassCB : register(b1);

groupshared float4 SharedPoints[4 + BLOCK_SIZE + 4];

static const float UniformFilter[9] =
{
	1.0f / 9, 1.0f / 9, 1.0f / 9, 1.0f / 9,  1.0f / 9, 1.0f / 9, 1.0f / 9, 1.0f / 9, 1.0f / 9
};

static const float GaussFilter[9] =
{
    0.0002, 0.0060, 0.0606, 0.2417, 0.3829, 0.2417, 0.0606, 0.0060, 0.0002
};

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, 1, 1)]
void Blur_HorizontalCS(CSInput input)
{
	uint3 globalCoords = input.DispatchThreadId;
	uint groupIndex = input.GroupIndex;
	uint3 groupThreadId = input.GroupThreadId;

	Texture2D inputTx = ResourceDescriptorHeap[PassCB.inputIdx];
	float4 data = inputTx.Load(globalCoords);

	SharedPoints[groupThreadId.x + 4] = data;
	if (groupIndex == 0)
	{
		SharedPoints[0] = inputTx.Load(globalCoords - int3(4, 0, 0));
		SharedPoints[1] = inputTx.Load(globalCoords - int3(3, 0, 0));
		SharedPoints[2] = inputTx.Load(globalCoords - int3(2, 0, 0));
		SharedPoints[3] = inputTx.Load(globalCoords - int3(1, 0, 0));
	}

	if (groupIndex == BLOCK_SIZE - 1)
	{
		SharedPoints[4 + BLOCK_SIZE + 0] = inputTx.Load(globalCoords + int3(1, 0, 0));
		SharedPoints[4 + BLOCK_SIZE + 1] = inputTx.Load(globalCoords + int3(2, 0, 0));
		SharedPoints[4 + BLOCK_SIZE + 2] = inputTx.Load(globalCoords + int3(3, 0, 0));
		SharedPoints[4 + BLOCK_SIZE + 3] = inputTx.Load(globalCoords + int3(4, 0, 0));
	}
	GroupMemoryBarrierWithGroupSync();

	float4 blurredColor = float4(0.0, 0.0, 0.0, 0.0);
	for (int x = 0; x < 9; x++) blurredColor += SharedPoints[groupThreadId.x + x] * GaussFilter[x];

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[globalCoords.xy] = blurredColor;
}

[numthreads(1, BLOCK_SIZE, 1)]
void Blur_VerticalCS(CSInput input)
{
	uint3 globalCoords = input.DispatchThreadId;
	uint groupIndex = input.GroupIndex;
	uint3 groupThreadId = input.GroupThreadId;

	Texture2D inputTx = ResourceDescriptorHeap[PassCB.inputIdx];
	float4 data = inputTx.Load(globalCoords);

	SharedPoints[groupThreadId.y + 4] = data;
	if (groupIndex == 0)
	{
		SharedPoints[0] = inputTx.Load(globalCoords - int3(0, 4, 0));
		SharedPoints[1] = inputTx.Load(globalCoords - int3(0, 3, 0));
		SharedPoints[2] = inputTx.Load(globalCoords - int3(0, 2, 0));
		SharedPoints[3] = inputTx.Load(globalCoords - int3(0, 1, 0));
	}

	if (groupIndex == BLOCK_SIZE - 1)
	{
		SharedPoints[4 + BLOCK_SIZE + 0] = inputTx.Load(globalCoords + int3(0, 1, 0));
		SharedPoints[4 + BLOCK_SIZE + 1] = inputTx.Load(globalCoords + int3(0, 2, 0));
		SharedPoints[4 + BLOCK_SIZE + 2] = inputTx.Load(globalCoords + int3(0, 3, 0));
		SharedPoints[4 + BLOCK_SIZE + 3] = inputTx.Load(globalCoords + int3(0, 4, 0));
	}
	GroupMemoryBarrierWithGroupSync();

	float4 blurredColor = float4(0.0, 0.0, 0.0, 0.0);
	for (int y = 0; y < 9; y++) blurredColor += SharedPoints[groupThreadId.y + y] * GaussFilter[y];

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[globalCoords.xy] = blurredColor;
}