#ifndef DDGICOMMON_INCLUDED
#define DDGICOMMON_INCLUDED
#include "../Packing.hlsli"

#define PROBE_IRRADIANCE_TEXELS 6
#define PROBE_DEPTH_TEXELS 14


struct DDGIVolume
{
	float3 startPosition;
	float normalBias;
	float3 probeSize;
	float energyPreservation;
	int3 probeCounts;
	int raysPerProbe;
	int maxRaysPerProbe;
	int irradianceIdx;
	int distanceIdx;
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

int3 BaseGridCoord(in DDGIVolume ddgi, float3 X)
{
	return clamp(int3((X - ddgi.startPosition) / ddgi.probeSize), int3(0, 0, 0), int3(ddgi.probeCounts) - int3(1, 1, 1));
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

float pow2(float x) { return x * x; }
float pow3(float x) { return x * x * x; }

float3 SampleDDGIIrradiance(in DDGIVolume ddgi, float3 P, float3 N, float3 Wo)
{
	Texture2D<float4> irradianceTexture = ResourceDescriptorHeap[volume.irradianceIdx];
	Texture2D<float2> distanceTexture = ResourceDescriptorHeap[volume.distanceIdx];

	int3 baseGridCoord = BaseGridCoord(ddgi, P);
	float3 baseProbePosition = GetProbeLocationFromGridCoord(ddgi, baseGridCoord);

	float3 sumIrradiance = 0.0f;
	float sumWeight = 0.0f;

	// alpha is how far from the floor(currentVertex) position. on [0, 1] for each axis.
	float3 alpha = clamp((P - baseProbePosition) / ddgi.probeSize, 0.0f, 1.0f);

	// Iterate over adjacent probe cage
	for (int i = 0; i < 8; ++i)
	{
		// Compute the offset grid coord and clamp to the probe grid boundary
		// Offset = 0 or 1 along each axis
		int3  offset = int3(i, i >> 1, i >> 2) & int3(1);
		int3  probeGridCoord = clamp(baseGridCoord + offset, int3(0), ddgi.probeCounts - int3(1));
		int p = GetProbeIndexFromGridCoord(ddgi, probeGridCoord);

		// Make cosine falloff in tangent plane with respect to the angle from the surface to the probe so that we never
		// test a probe that is *behind* the surface.
		// It doesn't have to be cosine, but that is efficient to compute and we must clip to the tangent plane.
		float3 probePosition = GetProbeLocationFromGridCoord(ddgi, probeGridCoord);

		// Bias the position at which visibility is computed; this
		// avoids performing a shadow test *at* a surface, which is a
		// dangerous location because that is exactly the line between
		// shadowed and unshadowed. If the normal bias is too small,
		// there will be light and dark leaks. If it is too large,
		// then samples can pass through thin occluders to the other
		// side (this can only happen if there are MULTIPLE occluders
		// near each other, a wall surface won't pass through itself.)
		float3 probeToPoint = P - probePosition + (N + 3.0 * Wo) * ddgi.normalBias;
		float3 dir = normalize(-probeToPoint);

		// Compute the trilinear weights based on the grid cell vertex to smoothly
		// transition between probes. Avoid ever going entirely to zero because that
		// will cause problems at the border probes. This isn't really a lerp. 
		// We're using 1-a when offset = 0 and a when offset = 1.
		float3 trilinear = lerp(1.0 - alpha, alpha, offset);
		float weight = 1.0;

		// Clamp all of the multiplies. We can't let the weight go to zero because then it would be 
		// possible for *all* weights to be equally low and get normalized
		// up to 1/n. We want to distinguish between weights that are 
		// low because of different factors.

		// Smooth backface test
		{
			// Computed without the biasing applied to the "dir" variable. 
			// This test can cause reflection-map looking errors in the image
			// (stuff looks shiny) if the transition is poor.
			float3 trueDirectionToProbe = normalize(probePosition - P);

			// The naive soft backface weight would ignore a probe when
			// it is behind the surface. That's good for walls. But for small details inside of a
			// room, the normals on the details might rule out all of the probes that have mutual
			// visibility to the point. So, we instead use a "wrap shading" test below inspired by
			// NPR work.
			// weight *= max(0.0001, dot(trueDirectionToProbe, wsN));

			// The small offset at the end reduces the "going to zero" impact
			// where this is really close to exactly opposite
			weight *= pow2(max(0.0001, (dot(trueDirectionToProbe, N) + 1.0) * 0.5)) + 0.2;
		}

		// Moment visibility test
		{
			float2 probeDepthUV = GetProbeUV(ddgi, probeGridCoord, -dir, PROBE_DEPTH_TEXELS);

			float distanceToProbe = length(probeToPoint);
			float2 moments = distanceTexture.SampleLevel(LinearClampSampler, probeDepthUV, 0).xy;

			float mean = moments.x;
			float variance = abs(pow2(moments.x) - moments.y);

			// http://www.punkuser.net/vsm/vsm_paper.pdf; equation 5
			// Need the max in the denominator because biasing can cause a negative displacement
			float chebyshevWeight = variance / (variance + pow2(max(distanceToProbe - mean, 0.0)));

			// Increase contrast in the weight 
			chebyshevWeight = max(pow3(chebyshevWeight), 0.0);

			weight *= (distanceToProbe <= mean) ? 1.0 : chebyshevWeight;
		}
		// Avoid zero weight
		weight = max(0.000001, weight);

		float3 irradianceDirection = N;
		float2 probeIrradianceUV = GetProbeUV(ddgi, probeGridCoord, normalize(irradianceDirection), PROBE_IRRADIANCE_TEXELS);
		float3 probeIrradiance = irradianceTexture.SampleLevel(LinearClampSampler, probeIrradianceUV, 0).rgb;

		// A tiny bit of light is really visible due to log perception, so
		// crush tiny weights but keep the curve continuous. This must be done
		// before the trilinear weights, because those should be preserved.
		const float crushThreshold = 0.2f;
		if (weight < crushThreshold)  weight *= weight * weight * (1.0f / pow2(crushThreshold));

		// Trilinear weights
		weight *= trilinear.x * trilinear.y * trilinear.z;

		// Weight in a more-perceptual brightness space instead of radiance space.
		// This softens the transitions between probes with respect to translation.
		// It makes little difference most of the time, but when there are radical transitions
		// between probes this helps soften the ramp.
#ifdef LINEAR_BLENDING
		probeIrradiance = sqrt(probeIrradiance);
#endif

		sumIrradiance += weight * probeIrradiance;
		sumWeight += weight;
	}

	float3 netIrradiance = sumIrradiance / sumWeight;

	netIrradiance.x = isnan(netIrradiance.x) ? 0.5f : netIrradiance.x;
	netIrradiance.y = isnan(netIrradiance.y) ? 0.5f : netIrradiance.y;
	netIrradiance.z = isnan(netIrradiance.z) ? 0.5f : netIrradiance.z;

	// Go back to linear irradiance
#ifdef LINEAR_BLENDING
	netIrradiance = pow2(netIrradiance);
#endif
	netIrradiance *= ddgi.energyPreservation;

	return 0.5f * M_PI * netIrradiance;
}

#endif