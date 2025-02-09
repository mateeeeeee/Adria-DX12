#ifndef _RANDOM_
#define _RANDOM_

//https://github.com/chris-wyman/GettingStartedWithRTXRayTracing/blob/master/05-AmbientOcclusion/Data/Tutorial05/hlslUtils.hlsli

struct RandomSamplerState
{
    uint seed;
};
typedef RandomSamplerState RNG;

RandomSamplerState RNG_Initialize(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	RandomSamplerState rng = (RandomSamplerState)0;
	rng.seed = v0;
	return rng;
}

float RNG_GetNext(inout RandomSamplerState rng)
{
	rng.seed = (1664525u * rng.seed + 1013904223u);
	return float(rng.seed & 0x00FFFFFF) / float(0x01000000);
}


#endif