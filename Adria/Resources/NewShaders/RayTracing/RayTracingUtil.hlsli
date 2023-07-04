#ifndef RAYTRACING_UTIL_INCLUDED
#define RAYTRACING_UTIL_INCLUDED

#include "../Common.hlsli"
#include "../Scene.hlsli"
#include "../Random.hlsli"
#include "../BRDF.hlsli"
#include "../Lighting.hlsli"
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

struct HitInfo
{
    float2   barycentricCoordinates;
    uint     primitiveIndex;
    uint     instanceIndex;
    float3x4 objectToWorldMatrix;
    float3x4 worldToObjectMatrix;
};

bool TraceRay(RayDesc ray, out HitInfo hitInfo)
{
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray);

    while (q.Proceed())
    {
		/*uint instanceIndex = q.CandidateInstanceID();
		uint triangleId = q.CandidatePrimitiveIndex();
		Instance instanceData = GetInstanceData(instanceIndex);
		Mesh meshData = GetMeshData(instanceData.meshIndex);
		Material materialData = GetMaterialData(instanceData.materialIdx);

		uint i0 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleId + 0);
		uint i1 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleId + 1);
		uint i2 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleId + 2);

		float2 uv0 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i0);
		float2 uv1 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i1);
		float2 uv2 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i2);
		float2 uv = Interpolate(uv0, uv1, uv2, q.CandidateTriangleBarycentrics());

		Texture2D txAlbedo = ResourceDescriptorHeap[materialData.diffuseIdx];
		float4 albedoColor = txAlbedo.SampleLevel(LinearWrapSampler, uv, 0) * float4(materialData.baseColorFactor, 1.0f);

		if (albedoColor.a >= materialData.alphaCutoff)*/
		q.CommitNonOpaqueTriangleHit();
    }
    if (q.CommittedStatus() == COMMITTED_NOTHING)
    {
        return false;
    }

    hitInfo.barycentricCoordinates = q.CommittedTriangleBarycentrics();
    hitInfo.primitiveIndex = q.CommittedPrimitiveIndex();
    hitInfo.instanceIndex = q.CommittedInstanceIndex();
    hitInfo.objectToWorldMatrix = q.CommittedObjectToWorld3x4();
    hitInfo.worldToObjectMatrix = q.CommittedWorldToObject3x4();
    return true;
}

bool TraceShadowRay(RayDesc ray)
{
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

    q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (q.Proceed())
    {
		switch (q.CandidateType())
        {
            case CANDIDATE_NON_OPAQUE_TRIANGLE:
		    {
                q.CommitNonOpaqueTriangleHit();
                break;
            }
        }
    }
    return q.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
}

bool TraceShadowRay(Light light, float3 worldPos)
{
	float3 direction;
	float maxT;
	switch (light.type)
	{
	case DIRECTIONAL_LIGHT:
		direction = -light.direction.xyz;
		maxT = FLT_MAX;
		break;
	case POINT_LIGHT:
		direction = light.position.xyz - worldPos;
		maxT = length(direction);
		break;
	case SPOT_LIGHT:
		direction = -light.direction.xyz;
		maxT = length(light.position.xyz - worldPos);
		break;
	}

	RayDesc ray;
	ray.Origin = worldPos;
	ray.Direction = normalize(direction);
	ray.TMin = 1e-1f;
	ray.TMax = maxT;

	return TraceShadowRay(ray);
}

bool TraceShadowRay(Light light, float3 worldPos, float4x4 inverseView)
{
	light.direction.xyz = mul(light.direction.xyz, (float3x3) inverseView);
	light.position = mul(float4(light.position.xyz, 1.0f), inverseView);
	light.position.xyz /= light.position.w;

	return TraceShadowRay(light, worldPos);
}

struct MaterialProperties
{
    float3 baseColor;
    float3 normalTS;
    float metallic;
    float3 emissive;
    float roughness;
    float opacity;
    float specular;
};

MaterialProperties GetMaterialProperties(Material material, float2 UV, int mipLevel)
{
    MaterialProperties properties = (MaterialProperties)0;
    float4 baseColor = float4(material.baseColorFactor, 1.0f);
    if (material.diffuseIdx >= 0)
    {
        baseColor *= SampleBindlessLevel2D(material.diffuseIdx, LinearWrapSampler, UV, mipLevel);
    }
    properties.baseColor = baseColor.rgb;
    properties.opacity = baseColor.a;

    properties.metallic = material.metallicFactor;
    properties.roughness = material.roughnessFactor;
    if (material.roughnessMetallicIdx >= 0)
    {
        float4 roughnessMetalnessSample = SampleBindlessLevel2D(material.roughnessMetallicIdx, LinearWrapSampler, UV, mipLevel);
        properties.metallic *= roughnessMetalnessSample.b;
        properties.roughness *= roughnessMetalnessSample.g;
    }
    properties.emissive = material.emissiveFactor.rrr;
    if (material.emissiveIdx >= 0)
    {
        properties.emissive *= SampleBindlessLevel2D(material.emissiveIdx, LinearWrapSampler, UV, mipLevel).rgb;
    }
    properties.specular = 0.5f;

    properties.normalTS = float3(0.5f, 0.5f, 1.0f);
    if (material.normalIdx >= 0)
    {
        properties.normalTS = SampleBindlessLevel2D(material.normalIdx, LinearWrapSampler, UV, mipLevel).rgb;
    }
    return properties;
}

BrdfData GetBrdfData(MaterialProperties material)
{
    BrdfData data;
    data.Diffuse = ComputeDiffuseColor(material.baseColor, material.metallic);
    data.Specular = ComputeF0(material.specular, material.baseColor, material.metallic);
    data.Roughness = material.roughness;
    return data;
}


#endif