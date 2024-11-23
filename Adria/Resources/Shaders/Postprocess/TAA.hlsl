#include "CommonResources.hlsli"

//https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/Antialiasing/TAA/TAA.ps.slang

#define BLOCK_SIZE 16

static const float Alpha = 0.1f;
static const float ColorBoxSigma = 1.0f;
static const int2 Offsets[8] =
{
	int2(-1, -1), int2(-1, 1),
	int2(1, -1), int2(1, 1),
	int2(1, 0), int2(0, -1),
	int2(0, 1), int2(-1, 0),
};

float3 BicubicSampleCatmullRom(Texture2D tex, float2 samplePos, float2 texDim)
{
	float2 texDimInv = 1.0 / texDim;
	float2 tc = floor(samplePos - 0.5) + 0.5;
	float2 f = samplePos - tc;
	float2 f2 = f * f;
	float2 f3 = f2 * f;

	float2 w0 = f2 - 0.5 * (f3 + f);
	float2 w1 = 1.5 * f3 - 2.5 * f2 + 1;
	float2 w3 = 0.5 * (f3 - f2);
	float2 w2 = 1 - w0 - w1 - w3;
	float2 w12 = w1 + w2;

	float2 tc0 = (tc - 1) * texDimInv;
	float2 tc12 = (tc + w2 / w12) * texDimInv;
	float2 tc3 = (tc + 2) * texDimInv;

	float3 result =
		tex.SampleLevel(LinearWrapSampler, float2(tc0.x, tc0.y), 0).rgb * (w0.x * w0.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc0.x, tc12.y), 0).rgb * (w0.x * w12.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc0.x, tc3.y), 0).rgb * (w0.x * w3.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc12.x, tc0.y), 0).rgb * (w12.x * w0.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc12.x, tc12.y), 0).rgb * (w12.x * w12.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc12.x, tc3.y), 0).rgb * (w12.x * w3.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc3.x, tc0.y), 0).rgb * (w3.x * w0.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc3.x, tc12.y), 0).rgb * (w3.x * w12.y) +
		tex.SampleLevel(LinearWrapSampler, float2(tc3.x, tc3.y), 0).rgb * (w3.x * w3.y);
	return result;
}
inline float3 RGBToYCgCo(float3 rgb)
{
	float Y = dot(rgb, float3(0.25f, 0.50f, 0.25f));
	float Cg = dot(rgb, float3(-0.25f, 0.50f, -0.25f));
	float Co = dot(rgb, float3(0.50f, 0.00f, -0.50f));
	return float3(Y, Cg, Co);
}
inline float3 YCgCoToRGB(float3 YCgCo)
{
	float tmp = YCgCo.x - YCgCo.y;
	float r = tmp + YCgCo.z;
	float g = YCgCo.x + YCgCo.y;
	float b = tmp - YCgCo.z;
	return float3(r, g, b);
}

struct TAAConstants
{
	uint sceneIdx;
	uint prevSceneIdx;
	uint velocityIdx;
	uint outputIdx;
};
ConstantBuffer<TAAConstants> TAAPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void TAA_CS(CSInput input)
{
	Texture2D<float4> sceneTexture		= ResourceDescriptorHeap[TAAPassCB.sceneIdx];
	Texture2D<float4> prevSceneTexture	= ResourceDescriptorHeap[TAAPassCB.prevSceneIdx];
	Texture2D<float2> velocityTexture	= ResourceDescriptorHeap[TAAPassCB.velocityIdx];
	RWTexture2D<float4> outputTexture	= ResourceDescriptorHeap[TAAPassCB.outputIdx];

	int2 pos = input.DispatchThreadId.xy;
	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);

	float3 color = sceneTexture.Load(int3(pos, 0)).rgb;
	color = RGBToYCgCo(color);
	float3 colorAvg = color;
	float3 colorVar = color * color;

	[unroll(8)]
	for (int i = 0; i < 8; i++)
	{
		float3 c = sceneTexture.Load(int3(pos + Offsets[i], 0)).rgb;
		c = RGBToYCgCo(c);
		colorAvg += c;
		colorVar += c * c;
	}
	const float oneOverNine = 1.0 / 9.0;
	colorAvg *= oneOverNine;
	colorVar *= oneOverNine;

	float3 sigma = sqrt(max(0.0f, colorVar - colorAvg * colorAvg));
	float3 colorMin = colorAvg - ColorBoxSigma * sigma;
	float3 colorMax = colorAvg + ColorBoxSigma * sigma;

	float2 motion = velocityTexture.Load(int3(pos, 0));
	[unroll(8)]
	for (int j = 0; j < 8; j++)
	{
		float2 m = velocityTexture.Load(int3(pos + Offsets[j], 0)).rg;
		motion = dot(m, m) > dot(motion, motion) ? m : motion;
	}

	float3 history = BicubicSampleCatmullRom(prevSceneTexture, (uv + motion) * FrameCB.renderResolution, FrameCB.renderResolution);
	history = RGBToYCgCo(history);

	float distToClamp = min(abs(colorMin.x - history.x), abs(colorMax.x - history.x));
	float alpha = clamp((Alpha * distToClamp) / (distToClamp + colorMax.x - colorMin.x), 0.0f, 1.0f);

	history = clamp(history, colorMin, colorMax);
	float3 result = YCgCoToRGB(lerp(history, color, alpha));
	outputTexture[input.DispatchThreadId.xy] = float4(result, 1.0f);
}