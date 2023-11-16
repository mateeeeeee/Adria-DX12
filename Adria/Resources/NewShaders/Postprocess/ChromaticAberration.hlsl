#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16

struct ChromaticAberrationConstants
{
	float intensity;
	uint  inputIdx;
	uint  outputIdx;
};
ConstantBuffer<ChromaticAberrationConstants> PassCB : register(b1);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ChromaticAberration(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	Texture2D<float4>	inputTx = ResourceDescriptorHeap[PassCB.inputIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	
	float2 uv = (dispatchThreadId.xy + 0.5f) /  FrameCB.renderResolution;
	float2 distortion = (uv - 0.5f) * PassCB.intensity /  FrameCB.renderResolution;

	float2 uv_R = uv + float2(1, 1) * distortion;
	float2 uv_G = uv + float2(0, 0) * distortion;
	float2 uv_B = uv - float2(1, 1) * distortion;

	float R = inputTx.SampleLevel(LinearClampSampler, uv_R, 0).r;
	float G = inputTx.SampleLevel(LinearClampSampler, uv_G, 0).g;
	float B = inputTx.SampleLevel(LinearClampSampler, uv_B, 0).b;
	
	outputTx[dispatchThreadId.xy] = float4(R, G, B, 1);
}