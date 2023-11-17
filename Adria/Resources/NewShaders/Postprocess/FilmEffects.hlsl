#include "../CommonResources.hlsli"
#include "../Constants.hlsli"

#define BLOCK_SIZE 16

struct FilmEffectsConstants
{
	bool  chromaticAberrationEnabled;
	float chromaticAberrationIntensity;
	
	bool  vignetteEnabled;
	float vignetteIntensity;

	uint  inputIdx;
	uint  outputIdx;
};

ConstantBuffer<FilmEffectsConstants> PassCB : register(b1);

float3 SampleWithChromaticAberration(float2 uv)
{
	Texture2D<float4>	inputTx = ResourceDescriptorHeap[PassCB.inputIdx];

	float3 color = 0.0f;
	if (PassCB.chromaticAberrationEnabled)
	{
		float2 distortion = (uv - 0.5f) * PassCB.chromaticAberrationIntensity / FrameCB.renderResolution;

		float2 uv_R = uv + float2(1, 1) * distortion;
		float2 uv_G = uv + float2(0, 0) * distortion;
		float2 uv_B = uv - float2(1, 1) * distortion;

		float R = inputTx.SampleLevel(LinearClampSampler, uv_R, 0).r;
		float G = inputTx.SampleLevel(LinearClampSampler, uv_G, 0).g;
		float B = inputTx.SampleLevel(LinearClampSampler, uv_B, 0).b;
		color = float3(R, G, B);
	}
	else
	{
		color = inputTx.SampleLevel(LinearClampSampler, uv, 0).rgb;
	}
	return color;
}

float3 ApplyVignette(float3 color, float2 uv)
{
	if (PassCB.vignetteEnabled)
	{
		const float2 uvCenter = float2(0.5f, 0.5f);
		float2 uvFromCenter = abs(uv - uvCenter) / float2(uvCenter);

		float2 vignetteMask = cos(uvFromCenter * PassCB.vignetteIntensity * M_PI_DIV_4);
		vignetteMask = vignetteMask * vignetteMask;
		vignetteMask = vignetteMask * vignetteMask;
		color *= clamp(vignetteMask.x * vignetteMask.y, 0, 1);
	}
	return color;
}



[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void FilmEffects(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	float2 uv = (dispatchThreadId.xy + 0.5f) /  FrameCB.renderResolution;
	float3 color = SampleWithChromaticAberration(uv);
	color = ApplyVignette(color, uv);

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[dispatchThreadId.xy] = float4(color, 1.0);
}
