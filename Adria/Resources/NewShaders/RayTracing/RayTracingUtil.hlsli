#pragma once

#include "../Common.hlsli"
#include "../Random.hlsli"
#include "../CommonResources.hlsli"

typedef BuiltInTriangleIntersectionAttributes HitAttributes;

float3 GetCosHemisphereSample(inout uint randSeed, float3 hitNorm)
{
	// Get a cosine-weighted random vector centered around a specified normal direction.
	// Get 2 random numbers to select our sample with
	float2 randVal = float2(NextRand(randSeed), NextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = GetPerpendicularVector(hitNorm);
	float3 tangent = cross(bitangent, hitNorm);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}
float3 GetConeSample(inout uint randSeed, float3 direction, float coneAngle)
{
	//https://medium.com/@alexander.wester/ray-tracing-soft-shadows-in-real-time-a53b836d123b
	float cosAngle = cos(coneAngle);

	float z = NextRand(randSeed) * (1.0f - cosAngle) + cosAngle;
	float phi = NextRand(randSeed) * 2.0f * 3.141592653589793238f;

	float x = sqrt(1.0f - z * z) * cos(phi);
	float y = sqrt(1.0f - z * z) * sin(phi);
	float3 north = float3(0.f, 0.f, 1.f);

	// Find the rotation axis `u` and rotation angle `rot` [1]
	float3 axis = normalize(cross(north, normalize(direction)));
	float angle = acos(dot(normalize(direction), north));

	// Convert rotation axis and angle to 3x3 rotation matrix [2]
	float3x3 R = AngleAxis3x3(angle, axis);

	return mul(R, float3(x, y, z));
}
float3 OffsetRay(const float3 p, const float3 n)
{
	/* A Fast and Robust Method for Avoiding
	Self-Intersection by Carsten Wächter and Nikolaus Binder
	Chapter 6. Ray Tracing Gems NVIDIA */

	static const float origin = 1.0f / 32.0f;
	static const float floatScale = 1.0f / 65536.0f;
	static const float intScale = 256.0f;

    int3 of_i = int3(intScale * n.x, intScale * n.y, intScale * n.z);
	float3 p_i = float3(
		asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
		asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
		asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x,
		abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y,
		abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

struct Vertex
{
	float3 pos;
	float2 uv;
	float3 nor;
	float3 tan;
	float3 bin;
};

float3 Interpolate(in float3 x0, in float3 x1, in float3 x2, float2 bary)
{
	return x0 * (1.0f - bary.x - bary.y) + bary.x * x1 + bary.y * x2;
}
float2 Interpolate(in float2 x0, in float2 x1, in float2 x2, float2 bary)
{
	return x0 * (1.0f - bary.x - bary.y) + bary.x * x1 + bary.y * x2;
}

struct MaterialData
{
    int albedoIdx;
    int normalIdx;
    int metallicRoughnessIdx;
    int emissiveIdx;

    float3 baseColor;
    float  metallicFactor;
    float  roughnessFactor;
    float  emissiveFactor;
    float  alphaCutoff;
};

struct GeoInfo
{
    uint vertexOffset;
    uint indexOffset;
    MaterialData materialData;
};

struct HitInfo
{
    float2   barycentricCoordinates;
    uint     primitiveIndex;
    uint     geometryIndex;
    float3x4 objectToWorldMatrix;
    float3x4 worldToObjectMatrix;
};

bool TraceRay(RayDesc ray, out HitInfo hitInfo)
{
    RaytracingAccelerationStructure raytracingAS = ResourceDescriptorHeap[FrameCB.accelStructIdx];
    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(raytracingAS, RAY_FLAG_NONE, 0xFF, ray);

    while (q.Proceed())
    {
        //uint instanceID = q.CandidateInstanceID();
        //uint primitiveIndex = q.CandidatePrimitiveIndex();
        //uint geometryIndex = q.CandidateGeometryIndex();
        //float2 barycentricCoordinates = q.CandidateTriangleBarycentrics();
        //todo alpha test
        q.CommitNonOpaqueTriangleHit(); 
    }
    if (q.CommittedStatus() == COMMITTED_NOTHING) 
    {
        return false;
    }

    hitInfo.barycentricCoordinates = q.CommittedTriangleBarycentrics();
    hitInfo.primitiveIndex = q.CommittedPrimitiveIndex();
    hitInfo.geometryIndex = q.CommittedGeometryIndex();
    hitInfo.objectToWorldMatrix = q.CommittedObjectToWorld3x4();
    hitInfo.worldToObjectMatrix = q.CommittedWorldToObject3x4();
    return true;
}

bool TraceShadowRay(RayDesc ray)
{
    RaytracingAccelerationStructure raytracingAS = ResourceDescriptorHeap[FrameCB.accelStructIdx];
    
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    q.TraceRayInline(raytracingAS, RAY_FLAG_NONE, 0xFF, ray);

    while (q.Proceed())
    {
        switch (q.CandidateType())
        {
            case CANDIDATE_NON_OPAQUE_TRIANGLE:
		    {
                q.CommitNonOpaqueTriangleHit();
                break;
            }
        //todo alpha test
        }
    }
    return q.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
}



