#include "../Constants.hlsli"
#include "../Lighting.hlsli"
#include "../Tonemapping.hlsli"
#include "../CommonResources.hlsli"
#include "../BRDF.hlsli"
#include "RayTracingUtil.hlsli"


#define MIN_BOUNCES 3
#define RIS_CANDIDATES_LIGHTS 8

#define RAY_INVALID -1
#define RAY_DIFFUSE 0
#define RAY_SPECULAR 1

struct PathTracingConstants
{
    int  bounceCount;
    int  accumulatedFrames;
    uint accumIdx;
    uint outputIdx;
    uint verticesIdx;
    uint indicesIdx;
    uint geoInfosIdx;
};
ConstantBuffer<PathTracingConstants> PassCB : register(b1);


struct PT_Payload
{
    bool   isHit;
};


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

MaterialProperties GetMaterialProperties(MaterialData material, float2 UV, int mipLevel)
{
    MaterialProperties properties = (MaterialProperties) 0;
    float4 baseColor = float4(material.baseColor, 1.0f);
    if (material.albedoIdx >= 0)
    {
        baseColor *= SampleBindlessLevel2D(material.albedoIdx, LinearWrapSampler, UV, mipLevel);
    }
    properties.baseColor = baseColor.rgb;
    properties.opacity = baseColor.a;
    
    properties.metallic = material.metallicFactor;
    properties.roughness = material.roughnessFactor;
    if (material.metallicRoughnessIdx >= 0)
    {
        float4 roughnessMetalnessSample = SampleBindlessLevel2D(material.metallicRoughnessIdx, LinearWrapSampler, UV, mipLevel);
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
    data.Diffuse = material.baseColor * (1.0f - material.metallic);
    data.Specular = lerp(0.08f * material.specular.xxx, material.baseColor, material.metallic);
    data.Roughness = material.roughness;
    return data;
}

void SampleSourceLight(in uint lightCount, inout uint seed, out uint lightIndex, out float sourcePdf);
bool SampleLightRIS(inout uint seed, float3 position, float3 N, out int lightIndex, out float sampleWeight);
LightingResult EvaluateLight(Light light, float3 worldPos, float3 V, float3 N, float3 geometryNormal, BrdfData brdfData);

[shader("raygeneration")]
void PT_RayGen()
{
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[FrameCB.accelStructIdx];
    RWTexture2D<float4> accumTx = ResourceDescriptorHeap[PassCB.accumIdx];

    float2 pixel = float2(DispatchRaysIndex().xy);
    float2 resolution = float2(DispatchRaysDimensions().xy);
    
    uint randSeed = InitRand(pixel.x + pixel.y * resolution.x, FrameCB.frameCount, 16);

    float2 offset = float2(NextRand(randSeed), NextRand(randSeed));
    pixel += lerp(-0.5f.xx, 0.5f.xx, offset);
    
    float2 ncdXY = (pixel / (resolution * 0.5f)) - 1.0f;
    ncdXY.y *= -1.0f;
    float4 rayStart = mul(float4(ncdXY, 0.0f, 1.0f), FrameCB.inverseViewProjection);
    float4 rayEnd = mul(float4(ncdXY, 1.0f, 1.0f), FrameCB.inverseViewProjection);

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz /= rayEnd.w;
    float3 rayDir = normalize(rayEnd.xyz - rayStart.xyz);
    float rayLength = length(rayEnd.xyz - rayStart.xyz);

    RayDesc ray;
    ray.Origin = rayStart.xyz;
    ray.Direction = rayDir;
    ray.TMin = 0.0f;
    ray.TMax = FLT_MAX;

    float3 radiance = 0.0f;
    float3 throughput = 1.0f;
    for (int i = 0; i < PassCB.bounceCount; ++i) 
    {
        PT_Payload payloadData;
        TraceRay(scene,
		 RAY_FLAG_FORCE_OPAQUE,
		 0xFF, 0, 0, 0, ray, payloadData);

        if (payloadData.isHit)
            radiance = float3(1, 0, 0);
        else
            radiance = float3(1, 1, 1);
        
        break;
    }

    float3 previousColor = accumTx[DispatchRaysIndex().xy].rgb;
    if (PassCB.accumulatedFrames > 1)
    {
        radiance += previousColor;
    }
    RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
    accumTx[DispatchRaysIndex().xy] = float4(radiance, 1.0f);
    outputTx[DispatchRaysIndex().xy] = float4(radiance, 1.0f) / PassCB.accumulatedFrames;
}

[shader("miss")]
void PT_Miss(inout PT_Payload payload_data)
{
    payload_data.isHit = false;
}


[shader("closesthit")]
void PT_ClosestHit(inout PT_Payload payloadData, in HitAttributes attribs)
{
    payloadData.isHit = true;
    /*StructuredBuffer<Vertex> vertices = ResourceDescriptorHeap[PassCB.verticesIdx];
    StructuredBuffer<uint> indices = ResourceDescriptorHeap[PassCB.indicesIdx];
    StructuredBuffer<GeoInfo> geoInfos = ResourceDescriptorHeap[PassCB.geoInfosIdx];
    StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];

    payloadData.isHit = true;
    uint geoId = GeometryIndex();
    uint triangleId = PrimitiveIndex();
    float2 barycentrics = attribs.barycentrics;
    
    GeoInfo geoInfo = geoInfos[geoId];
    uint vbOffset = geoInfo.vertexOffset;
    uint ibOffset = geoInfo.indexOffset;
    
    uint i0 = indices[ibOffset + triangleId * 3 + 0];
    uint i1 = indices[ibOffset + triangleId * 3 + 1];
    uint i2 = indices[ibOffset + triangleId * 3 + 2];
    
    Vertex v0 = vertices[vbOffset + i0];
    Vertex v1 = vertices[vbOffset + i1];
    Vertex v2 = vertices[vbOffset + i2];

    float3 localPosition = Interpolate(v0.pos, v1.pos, v2.pos, barycentrics);
    float2 uv = Interpolate(v0.uv, v1.uv, v2.uv, barycentrics);
    uv.y = 1.0f - uv.y;
    float3 localNormal = normalize(Interpolate(v0.nor, v1.nor, v2.nor, barycentrics));
    float3 worldPosition = mul(localPosition, ObjectToWorld3x4()).xyz;
    float3 worldNormal = mul(localNormal, (float3x3) WorldToObject4x3());
    float3 geometryNormal = worldNormal;
    float3 V = -WorldRayDirection();

    MaterialProperties matProperties = GetMaterialProperties(geoInfo.materialData, uv, 0);
    BrdfData brdfData = GetBrdfData(matProperties);
        //Direct light evaluation
    int lightIndex = 0;
    float lightWeight = 0.0f;
    if (SampleLightRIS(randSeed, worldPosition, worldNormal, lightIndex, lightWeight))
    {
        LightingResult result = EvaluateLight(lights[lightIndex], worldPosition, V, worldNormal, geometryNormal, brdfData);
        payloadData.radiance += payloadData.throughput * (result.Diffuse + result.Specular).xyz * lightWeight;
    }
    */

}


struct Reservoir
{
    uint LightSample;
    uint M;
    float TotalWeight;
    float SampleTargetPdf;
};

void SampleSourceLight(in uint lightCount, inout uint seed, out uint lightIndex, out float sourcePdf)
{
    lightIndex = min(uint(NextRand(seed) * lightCount), lightCount - 1);
    sourcePdf = 1.0f / lightCount;
}
bool SampleLightRIS(inout uint seed, float3 position, float3 N, out int lightIndex, out float sampleWeight)
{
    StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
    uint lightCount, _unused;
    lights.GetDimensions(lightCount, _unused);

    if (lightCount <= 0)
        return false;

    Reservoir reservoir;
    reservoir.TotalWeight = 0.0f;
    reservoir.M = RIS_CANDIDATES_LIGHTS;

    for (int i = 0; i < reservoir.M; ++i)
    {
        uint candidate = 0;
        float sourcePdf = 1.0f;
        SampleSourceLight(lightCount, seed, candidate, sourcePdf);

        Light light = lights[candidate];
        float3 positionDifference = light.position.xyz - position; //move to same space
        float distance = length(positionDifference);
        float3 L = positionDifference / distance;
        if (light.type == DIRECTIONAL_LIGHT)
        {
            L = -normalize(light.direction.xyz);
        }
        if (dot(N, L) < 0.0f) //N is world space, L view?
        {
            continue;
        }
        float targetPdf = Luminance(DoAttenuation(distance, light.range) * light.color.rgb);
        float risWeight = targetPdf / sourcePdf;
        reservoir.TotalWeight += risWeight;

        if (NextRand(seed) < (risWeight / reservoir.TotalWeight))
        {
            reservoir.LightSample = candidate;
            reservoir.SampleTargetPdf = targetPdf;
        }
    }

    if (reservoir.TotalWeight == 0.0f)
        return false;

    lightIndex = reservoir.LightSample;
    sampleWeight = (reservoir.TotalWeight / reservoir.M) / reservoir.SampleTargetPdf;
    return true;
}
LightingResult EvaluateLight(Light light, float3 worldPos, float3 V, float3 N, float3 geometryNormal, BrdfData brdfData)
{
    LightingResult result = (LightingResult) 0;

    float3 L = light.position.xyz - worldPos;
    float attenuation = DoAttenuation(length(L), light.range);
    if (light.type == DIRECTIONAL_LIGHT)
    {
        attenuation = 1.0f;
        L = -1000000.0f * light.direction.xyz;
    }
    //if (attenuation <= 0.0f)
    //{
    //    return result;
    //}

    RayDesc rayDesc;
    rayDesc.Origin = worldPos;
    rayDesc.Direction = normalize(L);
    rayDesc.TMin = 1e-2f;
    rayDesc.TMax = length(L);
    //RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
    //attenuation *= TraceOcclusionRay(rayDesc, tlas);

    L = normalize(L);
    result.Diffuse = float4(brdfData.Diffuse, 1.0f); //DefaultLitBxDF(brdfData.Specular, brdfData.Roughness, brdfData.Diffuse, N, V, L, attenuation);
    result.Diffuse *= light.color;
    result.Specular *= light.color;
    return result;
}
