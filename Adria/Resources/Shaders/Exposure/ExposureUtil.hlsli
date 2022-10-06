static float ComputeEV100(float aperture, float shutterSpeed, float ISO)
{
	// https://en.wikipedia.org/wiki/Exposure_value
	return log2((aperture * aperture / shutterSpeed) * (100.0 / ISO));
}
static float ComputeEV100(float avgLuminance)
{
	const float K = 12.5;
	return log2(avgLuminance * 100.0 / K);
}
static float ComputeExposure(float EV100)
{
	const float lensAttenuation = 0.65;
	const float lensImperfectionExposureScale = 78.0 / (100.0 * lensAttenuation); // 78 / ( S * q )

	float maxLuminance = lensImperfectionExposureScale * pow(2.0, EV100);
	return 1.0 / maxLuminance;
}
static float ComputeExposure(float aperture, float shutterSpeed, float ISO)
{
	float maxLuminance = (7800.0 / 65.0) * (aperture * aperture) / (shutterSpeed * ISO);
	return 1.0 / maxLuminance;
}
static float GetMeteringWeight(float2 screenPos, float2 screenSize)
{
#if METERING_MODE_SPOT
	const float screenDiagonal = 0.5f * (screenSize.x + screenSize.y);
	const float radius = 0.075 * screenDiagonal;
	const float2 center = screenSize * 0.5f;
	float d = length(center - screenPos) - radius;
	return 1.0 - saturate(d);
#elif METERING_MODE_CENTER_WEIGHTED
	const float screenDiagonal = 0.5f * (screenSize.x + screenSize.y);
	const float2 center = screenSize * 0.5f;
	//return 1.0 - saturate(pow(length(center - screenPos) * 2 / screenDiagonal, 0.5));
	return smoothstep(1.0, 0.0, length(center - screenPos) / (max(screenSize.x, screenSize.y) * 0.5));
#else //METERING_MODE_AVERAGE
	return 1.0;
#endif
}



#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16
#define HISTOGRAM_BIN_NUM (GROUP_SIZE_X * GROUP_SIZE_Y)

uint GetHistogramBin(float luminance, float minLuminance, float maxLuminance)
{
	float position = saturate((luminance - minLuminance) / (maxLuminance - minLuminance));
	return uint(pow(position, 1.0 / 5.0) * (HISTOGRAM_BIN_NUM - 1));
}
float Luminance(float3 color)
{
	return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}
float GetLuminance(uint histogramBin, float minLuminance, float maxLuminance)
{
	float position = ((float)histogramBin + 0.5) / float(HISTOGRAM_BIN_NUM - 1);
	return pow(position, 5.0) * (maxLuminance - minLuminance);
}
