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
