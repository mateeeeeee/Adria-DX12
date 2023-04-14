#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16

struct DoFConstants
{
	float4 dofParams;
	uint depthIdx;
	uint sceneIdx;
	uint blurredSceneIdx;
	uint outputIdx;
};
ConstantBuffer<DoFConstants> PassCB : register(b1);

float BlurFactor(in float depth, in float4 DOF)
{
	float f0 = 1.0f - saturate((depth - DOF.x) / max(DOF.y - DOF.x, 0.01f));
	float f1 = saturate((depth - DOF.z) / max(DOF.w - DOF.z, 0.01f));
	float blur = saturate(f0 + f1);

	return blur;
}
float BlurFactor2(in float depth, in float2 DOF)
{
	return saturate((depth - DOF.x) / (DOF.y - DOF.x));
}
float3 DistanceDOF(float3 colorFocus, float3 colorBlurred, float depth)
{
	float blurFactor = BlurFactor(depth, PassCB.dofParams);
	return lerp(colorFocus, colorBlurred, blurFactor);
}

struct CS_INPUT
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void DepthOfField(CS_INPUT input)
{
	Texture2D sceneTx = ResourceDescriptorHeap[PassCB.sceneIdx];
	Texture2D blurredSceneTx = ResourceDescriptorHeap[PassCB.blurredSceneIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);
	
	float depth = depthTx.Sample(LinearBorderSampler, uv);
	float3 color = sceneTx.Sample(LinearWrapSampler, uv).rgb;
	float3 colorBlurred = blurredSceneTx.Sample(LinearWrapSampler, uv).rgb;
	depth = LinearizeDepth(depth);
	outputTx[input.DispatchThreadId.xy] = float4(DistanceDOF(color.xyz, colorBlurred, depth), 1.0);
}