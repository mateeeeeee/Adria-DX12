#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16

struct FXAAConstants
{
	uint  ldrIdx;
	uint  outputIdx;
};
ConstantBuffer<FXAAConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void FXAA(CSInput input)
{
	Texture2D<float4> ldrTx = ResourceDescriptorHeap[PassCB.ldrIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);

	const float FXAA_SPAN_MAX = 8.0;
	const float FXAA_REDUCE_MUL = 1.0 / 8.0;
	const float FXAA_REDUCE_MIN = 1.0 / 128.0;

	float3 rgbNW = ldrTx.Sample(LinearWrapSampler, uv + float2(-1.0, -1.0) / FrameCB.screenResolution, 0).rgb;
	float3 rgbNE = ldrTx.Sample(LinearWrapSampler, uv + float2(1.0, -1.0)  / FrameCB.screenResolution, 0).rgb;
	float3 rgbSW = ldrTx.Sample(LinearWrapSampler, uv + float2(-1.0, 1.0)  / FrameCB.screenResolution, 0).rgb;
	float3 rgbSE = ldrTx.Sample(LinearWrapSampler, uv + float2(1.0, 1.0)   / FrameCB.screenResolution, 0).rgb;
	float3 rgbM  = ldrTx.Sample(LinearWrapSampler, uv, 0).rgb;

	float3 luma = float3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot(rgbM, luma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	float2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max(
		(lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
		FXAA_REDUCE_MIN);

	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

	dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
		max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
			dir * rcpDirMin)) / FrameCB.screenResolution;

	float3 rgbA = (1.0 / 2.0) *
		(
			ldrTx.Sample(LinearWrapSampler, uv + dir * (1.0 / 3.0 - 0.5), 0).rgb +
			ldrTx.Sample(LinearWrapSampler, uv + dir * (2.0 / 3.0 - 0.5), 0).rgb
			);

	float3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) *
		(
			ldrTx.Sample(LinearWrapSampler, uv + dir * (0.0 / 3.0 - 0.5), 0).rgb +
			ldrTx.Sample(LinearWrapSampler, uv + dir * (3.0 / 3.0 - 0.5), 0).rgb
			);

	float lumaB = dot(rgbB, luma);

	float3 finalColor;
	if ((lumaB < lumaMin) || (lumaB > lumaMax))
	{
		finalColor.xyz = rgbA;
	}
	else
	{
		finalColor.xyz = rgbB;
	}
	outputTx[input.DispatchThreadId.xy] = float4(finalColor, 1.0f);
}