#ifndef _DDGI_COMMON_
#define _DDGI_COMMON_

#include "Packing.hlsli"
#include "Constants.hlsli"
#include "Common.hlsli"
#include "CommonResources.hlsli"

#define PROBE_IRRADIANCE_TEXELS 6
#define PROBE_DISTANCE_TEXELS 14
#define BACKFACE_DEPTH_MULTIPLIER -0.2f
#define MIN_WEIGHT 0.0001f


struct DDGIVolume
{
	float3 startPosition;
	int raysPerProbe;
	float3 probeSize;
	int maxRaysPerProbe;
	int3 probeCounts;
	float normalBias;
	float energyPreservation;
	int irradianceHistoryIdx;
	int distanceHistoryIdx;
};

uint2 GetProbeTexel(DDGIVolume ddgi, uint3 probeIndex3D, uint numProbeInteriorTexels)
{
	uint numProbeTexels = 1 + numProbeInteriorTexels + 1;
	return uint2(probeIndex3D.x + probeIndex3D.y * ddgi.probeCounts.x, probeIndex3D.z) * numProbeTexels + 1;
}

float2 GetProbeUV(DDGIVolume volume, uint3 probeIndex3D, float3 direction, uint numProbeInteriorTexels)
{
	uint numProbeTexels = 1 + numProbeInteriorTexels + 1;
	uint textureWidth = numProbeTexels * volume.probeCounts.x * volume.probeCounts.y;
	uint textureHeight = numProbeTexels * volume.probeCounts.z;

	float2 pixel = GetProbeTexel(volume, probeIndex3D, numProbeInteriorTexels);
	pixel += (EncodeNormalOctahedron(normalize(direction)) * 0.5f + 0.5f) * numProbeInteriorTexels;
	return pixel / float2(textureWidth, textureHeight);
}

int GetProbeIndexFromGridCoord(in DDGIVolume ddgi, uint3 gridCoord)
{
	return int(gridCoord.x + gridCoord.y * ddgi.probeCounts.x + gridCoord.z * ddgi.probeCounts.x * ddgi.probeCounts.y);
}

float3 GetProbeLocationFromGridCoord(in DDGIVolume ddgi, uint3 gridCoord)
{
	return ddgi.startPosition + ddgi.probeSize * gridCoord;
}

uint3 GetProbeGridCoord(in DDGIVolume ddgi, uint probeIndex)
{
	uint3 gridCoord;
	gridCoord.x = probeIndex % ddgi.probeCounts.x;
	gridCoord.y = (probeIndex % (ddgi.probeCounts.x * ddgi.probeCounts.y)) / ddgi.probeCounts.x;
	gridCoord.z = probeIndex / (ddgi.probeCounts.x * ddgi.probeCounts.y);
	return gridCoord;
}

float3 GetProbeLocation(in DDGIVolume ddgi, uint probeIndex)
{
	uint3 gridCoord = GetProbeGridCoord(ddgi, probeIndex);
	return GetProbeLocationFromGridCoord(ddgi, gridCoord);
}

uint3 BaseGridCoord(in DDGIVolume ddgi, float3 X)
{
	return clamp(uint3((X - ddgi.startPosition) / ddgi.probeSize), uint3(0, 0, 0), uint3(ddgi.probeCounts) - uint3(1, 1, 1));
}

// Ray Tracing Gems 2: Essential Ray Generation Shaders
float3 SphericalFibonacci(float i, float n)
{
	const float PHI = sqrt(5) * 0.5f + 0.5f;
	float fraction = (i * (PHI - 1)) - floor(i * (PHI - 1));
	float phi = 2.0f * M_PI * fraction;
	float cosTheta = 1.0f - (2.0f * i + 1.0f) * (1.0f / n);
	float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
	return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3 GetRayDirection(uint rayIndex, uint numRays, float3x3 randomRotation = float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1))
{
	return mul(SphericalFibonacci(rayIndex, numRays), randomRotation);
}

float3 ComputeBias(DDGIVolume volume, float3 normal, float3 viewDirection, float b = 0.2f)
{
	const float normalBiasMultiplier = 0.2f;
	const float viewBiasMultiplier = 0.8f;
	const float axialDistanceMultiplier = 0.75f;
	return (normal * normalBiasMultiplier + viewDirection * viewBiasMultiplier) * axialDistanceMultiplier * Min(volume.probeSize) * b;
}

float Pow2(float x) { return x * x; }
float Pow3(float x) { return x * x * x; }

//https://github.com/simco50/D3D12_Research/blob/master/D3D12/Resources/Shaders/RayTracing/DDGICommon.hlsli
float3 SampleDDGIIrradiance(in DDGIVolume ddgi, float3 P, float3 N, float3 Wo)
{
	Texture2D<float4> irradianceTexture = ResourceDescriptorHeap[ddgi.irradianceHistoryIdx];
	Texture2D<float2> distanceTexture = ResourceDescriptorHeap[ddgi.distanceHistoryIdx];

	float3 direction = N;
	float3 position = P;
	float  volumeWeight = 1.0f;

	float3 relativeCoordinates = (position - ddgi.startPosition) / ddgi.probeSize;
	for(uint i = 0; i < 3; ++i)
	{
		volumeWeight *= lerp(0, 1, saturate(relativeCoordinates[i]));
		if(relativeCoordinates[i] > ddgi.probeCounts[i] - 2)
		{
			float x = saturate(relativeCoordinates[i] - ddgi.probeCounts[i] + 2);
			volumeWeight *= lerp(1, 0, x);
		}
	}
	if(volumeWeight <= 0.0f) return 0.0f;

	position += ComputeBias(ddgi, direction, -Wo, 0.2f);

	uint3 baseProbeCoordinates = floor(relativeCoordinates);
	float3 baseProbePosition = GetProbeLocationFromGridCoord(ddgi, baseProbeCoordinates);
	float3 alpha = saturate((position - baseProbePosition) / ddgi.probeSize);

	float3 sumIrradiance = 0;
	float sumWeight = 0;

	for (uint i = 0; i < 8; ++i)
	{
		uint3 indexOffset = uint3(i, i >> 1u, i >> 2u) & 1u;

		uint3 probeCoordinates = clamp(baseProbeCoordinates + indexOffset, 0, ddgi.probeCounts - 1);
		float3 probePosition = GetProbeLocationFromGridCoord(ddgi, probeCoordinates);

		float3 relativeProbePosition = position - probePosition;
		float3 probeDirection = -normalize(relativeProbePosition);

		float3 trilinear = max(0.001f, lerp(1.0f - alpha, alpha, indexOffset));
        float trilinearWeight = (trilinear.x * trilinear.y * trilinear.z);

		float weight = 1;

		weight *= saturate(dot(probeDirection, direction));

		float2 distanceUV = GetProbeUV(ddgi, probeCoordinates, -probeDirection, PROBE_DISTANCE_TEXELS);
		float probeDistance = length(relativeProbePosition);
		// https://developer.download.nvidia.com/SDK/10/direct3d/Source/VarianceShadowMapping/Doc/VarianceShadowMapping.pdf
		float2 moments = distanceTexture.SampleLevel(LinearClampSampler, distanceUV, 0).xy;
		float variance = abs(Pow2(moments.x) - moments.y);
		float chebyshev = 1.0f;
		if(probeDistance > moments.x)
		{
			float mD = moments.x - probeDistance;
			chebyshev = variance / (variance + Pow2(mD));
			chebyshev = max(Pow3(chebyshev), 0.0);
		}
		weight *= max(chebyshev, 0.05f);
		weight = max(0.000001f, weight);

		const float crushThreshold = 0.2f;
		if (weight < crushThreshold)
		{
			weight *= weight * weight * (1.0f / Pow2(crushThreshold));
		}
		weight *= trilinearWeight;

		float2 uv = GetProbeUV(ddgi, probeCoordinates, direction, PROBE_IRRADIANCE_TEXELS);
		float3 irradiance = irradianceTexture.SampleLevel(LinearClampSampler, uv, 0).rgb;
		irradiance = pow(irradiance, 2.5f);

		sumIrradiance += irradiance * weight;
		sumWeight += weight;
	}
	
	if(sumWeight == 0) return 0.0f;

	sumIrradiance *= (1.0f / sumWeight);
	sumIrradiance *= sumIrradiance;
	sumIrradiance *= M_PI;
	return sumIrradiance * volumeWeight;

}

#endif