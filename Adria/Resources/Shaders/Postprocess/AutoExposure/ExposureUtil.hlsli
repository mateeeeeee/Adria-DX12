#ifndef _EXPOSURE_UTIL_
#define _EXPOSURE_UTIL_
#include "Tonemapping.hlsli"

#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16
#define HISTOGRAM_BIN_NUM 256


float ComputeEV100(float avgLuminance)
{
	const float K = 12.5f;
	const float ISO = 100.0f;
	return log2(avgLuminance * ISO / K);
}
float ComputeExposure(float EV100)
{
	return 1.0f / (pow(2.0f, EV100) * 1.2f);
}

uint GetHistogramBin(float3 color, float minLogLuminance, float logLuminanceRangeRcp)
{
	float luminance = Luminance(color);
	if(luminance < 1e-5f)
	{
		return 0;
	}

	float logLuminance = saturate((log2(luminance) - minLogLuminance) * logLuminanceRangeRcp);
	return (uint)(logLuminance * (HISTOGRAM_BIN_NUM - 1) + 1.0);
}

float Adaption(float current, float target, float dt, float speed)
{
	float factor = 1.0f - exp2(-dt * speed);
	return current + (target - current) * factor;
}

#endif