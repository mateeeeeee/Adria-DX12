#include "../CommonResources.hlsli"
#include "../Common.hlsli"

#define BLOCK_SIZE 16

struct LensFlareConstants
{
	float2 sunScreenSpacePosition;
	uint   depthIdx;
	uint   outputIdx;
};
ConstantBuffer<LensFlareConstants> PassCB : register(b1);

//https://www.shadertoy.com/view/4sX3Rs
float3 LensFlare(float2 uv, float2 pos)
{
	uv = 2.0f * uv - 1.0f;

	float intensity = 1.5;
	float2 main = uv - pos;
	float2 uvd = uv * (length(uv));

	float dist = length(main); dist = pow(dist, .1);

	float f1 = max(0.01 - pow(length(uv + 1.2 * pos), 1.9), .0) * 7.0;

	float f2 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.8 * pos), 2.0)), .0) * 00.1;
	float f22 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.85 * pos), 2.0)), .0) * 00.08;
	float f23 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.9 * pos), 2.0)), .0) * 00.06;

	float2 uvx = lerp(uv, uvd, -0.5);

	float f4 = max(0.01 - pow(length(uvx + 0.4 * pos), 2.4), .0) * 6.0;
	float f42 = max(0.01 - pow(length(uvx + 0.45 * pos), 2.4), .0) * 5.0;
	float f43 = max(0.01 - pow(length(uvx + 0.5 * pos), 2.4), .0) * 3.0;

	uvx = lerp(uv, uvd, -.4);

	float f5 = max(0.01 - pow(length(uvx + 0.2 * pos), 5.5), .0) * 2.0;
	float f52 = max(0.01 - pow(length(uvx + 0.4 * pos), 5.5), .0) * 2.0;
	float f53 = max(0.01 - pow(length(uvx + 0.6 * pos), 5.5), .0) * 2.0;

	uvx = lerp(uv, uvd, -0.5);

	float f6 = max(0.01 - pow(length(uvx - 0.3 * pos), 1.6), .0) * 6.0;
	float f62 = max(0.01 - pow(length(uvx - 0.325 * pos), 1.6), .0) * 3.0;
	float f63 = max(0.01 - pow(length(uvx - 0.35 * pos), 1.6), .0) * 5.0;

	float3 c = 0.0f;

	c.r += f2 + f4 + f5 + f6; c.g += f22 + f42 + f52 + f62; c.b += f23 + f43 + f53 + f63;
	c = c * 1.3 - length(uvd) * 0.05;

	float3 color = c * intensity;
	float w = color.x + color.y + color.z;
	return lerp(color, w * 0.5f, w * 0.1f);
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void LensFlareCS(CSInput input)
{
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

	uint3 threadId = input.DispatchThreadId;
	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
	float2 sunPos = 2.0f * PassCB.sunScreenSpacePosition - 1.0f;

	//#todo maybe check occlusion queries?
	const float2 step = 1.0f / FrameCB.displayResolution;
	const float2 range = 3.5f * step;
	float samples = 0.0f;
	float visibility = 0.0f;
	for (float y = -range.y; y <= range.y; y += step.y)
	{
		for (float x = -range.x; x <= range.x; x += step.x)
		{
			samples += 1.0f;
			float2 depthUv = PassCB.sunScreenSpacePosition.xy + float2(x, y);
			if (IsSaturated(depthUv)) visibility += depthTx.SampleLevel(PointClampSampler, depthUv, 0).r < 1.0f ? 0 : 1;
		}
	}
	visibility /= samples;

	float3 lensFlareColor = max(LensFlare(uv, sunPos), 0.0f) * visibility;
	outputTx[threadId.xy] += float4(lensFlareColor, 1.0f);
}