#include "CommonResources.hlsli"
#include "Random.hlsli"

#define BLOCK_SIZE 16

struct RainSplashConstants
{
	uint   gbufferDiffuseIdx;
	uint   gbufferNormalIdx;
	uint   rainSplashDiffuseIdx;
	uint   rainSplashNormalIdx;
};
ConstantBuffer<RainSplashConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void RainSplashCS(CSInput input)
{
	uint3 dispatchThreadId = input.DispatchThreadId;
	
	RWTexture2D<float4> gbufferDiffuse = ResourceDescriptorHeap[PassCB.gbufferDiffuseIdx];
	Texture3D<float4>   rainSplashDiffuse = ResourceDescriptorHeap[PassCB.rainSplashDiffuseIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
	float3 splash = rainSplashDiffuse.Sample(LinearMirrorSampler, float3(uv, FrameCB.totalTime)).rgb;
	gbufferDiffuse[dispatchThreadId.xy].rgb = splash;
}