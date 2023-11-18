#include "../CommonResources.hlsli"
#include "../Constants.hlsli"
#include "../Noise.hlsli"

#define BLOCK_SIZE 16

struct FilmEffectsConstants
{
	bool  lensDistortionEnabled;
	float lensDistortionIntensity;

	bool  chromaticAberrationEnabled;
	float chromaticAberrationIntensity;
	
	bool  vignetteEnabled;
	float vignetteIntensity;

	bool  filmGrainEnabled;
	float filmGrainScale;
	float filmGrainAmount;
	uint  filmGrainSeed;

	uint  inputIdx;
	uint  outputIdx;
};

ConstantBuffer<FilmEffectsConstants> PassCB : register(b2);

float2 ApplyLensDistortion(float2 uv)
{
	if (PassCB.lensDistortionEnabled)
	{
		const float2 center = float2(0.5, 0.5);
		float2 distortionVector = uv - center;
		float  distortionRadius = length(distortionVector);
		float  distortionFactor = 1.0 + PassCB.lensDistortionIntensity * distortionRadius * distortionRadius;
		uv = center + distortionVector * distortionFactor;
	}
	return uv;
}

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

		float R = inputTx.SampleLevel(LinearBorderSampler, uv_R, 0).r;
		float G = inputTx.SampleLevel(LinearBorderSampler, uv_G, 0).g;
		float B = inputTx.SampleLevel(LinearBorderSampler, uv_B, 0).b;
		color = float3(R, G, B);
	}
	else
	{
		color = inputTx.SampleLevel(LinearBorderSampler, uv, 0).rgb;
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

float3 ApplyFilmGrain(float3 color, uint2 coord)
{
	if (PassCB.filmGrainEnabled)
	{
		float  filmGrainScale	= PassCB.filmGrainScale;
		float  filmGrainAmount	= PassCB.filmGrainAmount;
		uint   filmGrainSeed	= PassCB.filmGrainSeed;

		float2     randomNumberFine = PCG3D16(uint3(coord / (filmGrainScale / 8.0), filmGrainSeed)).xy;
		float2     simplex = Simplex(coord / filmGrainScale + randomNumberFine);

		const float grainShape = 3.0;
		float grain = 1 - 2 * exp2(-length(simplex) * grainShape);
		color += grain * min(color, 1 - color) * filmGrainAmount;
	}
	return color;
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void FilmEffects(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	float2 uv = (dispatchThreadId.xy + 0.5f) /  FrameCB.renderResolution;
	uv = ApplyLensDistortion(uv);
	float3 color = SampleWithChromaticAberration(uv);
	color = ApplyVignette(color, uv);
	color = ApplyFilmGrain(color, dispatchThreadId.xy);

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[dispatchThreadId.xy] = float4(color, 1.0);
}
