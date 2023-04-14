#pragma once

static const float BayerFactor = 1.0f / 16.0f;
static const float BayerFilter[16] =
{
    0.0f * BayerFactor, 8.0f * BayerFactor, 2.0f * BayerFactor, 10.0f * BayerFactor,
	12.0f * BayerFactor, 4.0f * BayerFactor, 14.0f * BayerFactor, 6.0f * BayerFactor,
	3.0f * BayerFactor, 11.0f * BayerFactor, 1.0f * BayerFactor, 9.0f * BayerFactor,
	15.0f * BayerFactor, 7.0f * BayerFactor, 13.0f * BayerFactor, 5.0f * BayerFactor
};
static float3 NoiseKernelConeSampling[6] =
{
    float3(0.38051305, 0.92453449, -0.02111345),
	float3(-0.50625799, -0.03590792, -0.86163418),
	float3(-0.32509218, -0.94557439, 0.01428793),
	float3(0.09026238, -0.27376545, 0.95755165),
	float3(0.28128598, 0.42443639, -0.86065785),
	float3(-0.16852403, 0.14748697, 0.97460106)
};

static float BeerLaw(float d)
{
    return exp(-d);
}
static float SugarPowder(float d)
{
    return (1.0f - exp(-2.0f * d));
}
static float HenyeyGreenstein(float sundotrd, float g)
{
    float gg = g * g;
    return (1.0f - gg) / pow(1.0f + gg - 2.0f * g * sundotrd, 1.5f);
}
static float LightEnergy(float d, float cos_angle)
{
    return 5.0 * BeerLaw(d) * SugarPowder(d) * HenyeyGreenstein(cos_angle, 0.2f);
}
static float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
    return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

