#pragma once

static const float BayerFactor = 1.0f / 16.0f;
static const int BayerFilter[16] =
{
    0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
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

