#ifndef DDGICOMMON_INCLUDED
#define DDGICOMMON_INCLUDED

struct DDGIVolume
{
	float3 startPosition;
	float3 probeSize;
	int3 probeCounts;
	int raysPerProbe;
	int maxRaysPerProbe;
};

int GetProbeIndexFromGridCoord(in DDGIVolume ddgi, int3 gridCoord)
{
	return int(gridCoord.x + gridCoord.y * ddgi.probeCounts.x + gridCoord.z * ddgi.probeCounts.x * ddgi.probeCounts.y);
}

float3 GetProbeLocationFromGridCoord(in DDGIVolume ddgi, int3 gridCoord)
{
	return ddgi.startPosition + ddgi.probeSize * gridCoord;
}

int3 GetProbeGridCoord(in DDGIVolume ddgi, int probeIndex)
{
	int3 gridCoord;
	gridCoord.x = probeIndex % ddgi.probeCounts.x;
	gridCoord.y = (probeIndex % (ddgi.probeCounts.x * ddgi.probeCounts.y)) / ddgi.probeCounts.x;
	gridCoord.z = probeIndex / (ddgi.probeCounts.x * ddgi.probeCounts.y);
	return gridCoord;
}

float3 GetProbeLocation(in DDGIVolume ddgi, int probeIndex)
{
	int3 gridCoord = GetProbeGridCoord(ddgi, probeIndex);
	return GetProbeLocationFromGridCoord(ddgi, gridCoord);
}

// Ray Tracing Gems 2: Essential Ray Generation Shaders
float3 SphericalFibonacci(float i, float n)
{
	const float PHI = sqrt(5) * 0.5f + 0.5f;
	float fraction = (i * (PHI - 1)) - floor(i * (PHI - 1));
	float phi = 2.0f * PI * fraction;
	float cosTheta = 1.0f - (2.0f * i + 1.0f) * (1.0f / n);
	float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
	return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

#endif