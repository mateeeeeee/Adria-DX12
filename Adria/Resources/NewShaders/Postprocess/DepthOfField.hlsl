#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16

struct DoFConstants
{
	float focusDistance;
	float focusRadius;
	uint depthIdx;
	uint sceneIdx;
	uint blurredSceneIdx;
	uint outputIdx;
};
ConstantBuffer<DoFConstants> PassCB : register(b1);

float BlurFactor(in float depth)
{
	return saturate(abs(depth - PassCB.focusDistance) / PassCB.focusRadius);
}

float3 DistanceDOF(float3 colorFocus, float3 colorBlurred, float depth)
{
	float blurFactor = BlurFactor(depth);
	return lerp(colorFocus, colorBlurred, blurFactor);
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void DepthOfField(CSInput input)
{
	Texture2D sceneTx = ResourceDescriptorHeap[PassCB.sceneIdx];
	Texture2D blurredSceneTx = ResourceDescriptorHeap[PassCB.blurredSceneIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
	
	float depth = depthTx.Sample(LinearBorderSampler, uv);
	float3 color = sceneTx.Sample(LinearWrapSampler, uv).rgb;
	float3 colorBlurred = blurredSceneTx.Sample(LinearWrapSampler, uv).rgb;
	depth = LinearizeDepth(depth);
	outputTx[input.DispatchThreadId.xy] = float4(DistanceDOF(color.xyz, colorBlurred, depth), 1.0);
}