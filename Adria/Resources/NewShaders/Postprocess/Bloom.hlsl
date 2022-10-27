#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16

struct BloomExtractConstants
{
	uint inputIdx;
	uint outputIdx;

	float threshold;
	float scale;
};
ConstantBuffer<BloomExtractConstants> PassCB : register(b1);

struct CS_INPUT
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BloomExtract(CS_INPUT input)
{
	Texture2D inputTx = ResourceDescriptorHeap[PassCB.inputIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	
	float3 color = inputTx[input.DispatchThreadId.xy].rgb;
	color = min(color, 10.0f);
	color = max(color - PassCB.threshold, 0.0f);
	outputTx[input.DispatchThreadId.xy] = float4(PassCB.scale * color, 1.0f);
}

struct BloomCombineConstants
{
	uint inputIdx;
	uint outputIdx;
	uint bloomIdx;
};
ConstantBuffer<BloomCombineConstants> PassCB2 : register(b1);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BloomCombine(CS_INPUT input)
{
	Texture2D inputTx = ResourceDescriptorHeap[PassCB2.inputIdx];
	Texture2D bloomTx = ResourceDescriptorHeap[PassCB2.bloomIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB2.outputIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);

	float3 bloom = bloomTx.SampleLevel(LinearClampSampler, uv, 1.5f).xyz;
	bloom += bloomTx.SampleLevel(LinearClampSampler, uv, 2.5f).xyz;
	bloom += bloomTx.SampleLevel(LinearClampSampler, uv, 3.5f).xyz;
	bloom /= 3;
	outputTx[input.DispatchThreadId.xy] = inputTx[input.DispatchThreadId.xy] + float4(bloom, 0.0f);
}