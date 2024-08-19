#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

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

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

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