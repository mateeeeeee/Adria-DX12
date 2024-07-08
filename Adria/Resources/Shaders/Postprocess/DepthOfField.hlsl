#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct DepthOfFieldConstants
{
	float focusDistance;
	float focusRadius;
	uint depthIdx;
	uint sceneIdx;
	uint blurredSceneIdx;
	uint outputIdx;
};
ConstantBuffer<DepthOfFieldConstants> DepthOfFieldPassCB : register(b1);

float BlurFactor(in float depth)
{
	return saturate(abs(depth - DepthOfFieldPassCB.focusDistance) / DepthOfFieldPassCB.focusRadius);
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
void DepthOfFieldCS(CSInput input)
{
	Texture2D sceneTexture = ResourceDescriptorHeap[DepthOfFieldPassCB.sceneIdx];
	Texture2D blurredSceneTexture = ResourceDescriptorHeap[DepthOfFieldPassCB.blurredSceneIdx];
	Texture2D<float> depthTexture = ResourceDescriptorHeap[DepthOfFieldPassCB.depthIdx];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[DepthOfFieldPassCB.outputIdx];
	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
	
	float depth = depthTexture.Sample(LinearBorderSampler, uv);
	float3 color = sceneTexture.Sample(LinearWrapSampler, uv).rgb;
	float3 colorBlurred = blurredSceneTexture.Sample(LinearWrapSampler, uv).rgb;
	depth = LinearizeDepth(depth);
	outputTexture[input.DispatchThreadId.xy] = float4(DistanceDOF(color.xyz, colorBlurred, depth), 1.0);
}