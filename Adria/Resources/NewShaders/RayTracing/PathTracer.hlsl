#include "../Constants.hlsli"
#include "../Lighting.hlsli"
#include "../Tonemapping.hlsli"
#include "../CommonResources.hlsli"
#include "RayTracingUtil.hlsli"

#define RIS_CANDIDATES_LIGHTS 8

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
    bool isHit;
    uint geoId;
    uint triangleId;
    float2 barycentrics;
};


// From RT Gems 2
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
	// [Algorithm]
	// Pick N random lights from the full set of lights
	// Compute contribution of each light
	// If the light's weight is above a random threshold, pick it
	// Weight the selected light based on the total weight and light count

    if (lightCount <= 0) return false;

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

[shader("raygeneration")]
void PT_RayGen()
{
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[FrameCB.accelStructIdx];
    RWTexture2D<float4> accumTx = ResourceDescriptorHeap[PassCB.accumIdx];
    TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
    StructuredBuffer<Vertex> vertices = ResourceDescriptorHeap[PassCB.verticesIdx];
    StructuredBuffer<uint> indices = ResourceDescriptorHeap[PassCB.indicesIdx];
    StructuredBuffer<GeoInfo> geoInfos = ResourceDescriptorHeap[PassCB.geoInfosIdx];

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
        
        if (!payloadData.isHit)
        {
            radiance += throughput * envMap.SampleLevel(LinearWrapSampler, ray.Direction, 0).rgb;
            break;
        }

        uint geoId = payloadData.geoId;
        uint triangleId = payloadData.triangleId;
        float2 barycentrics = payloadData.barycentrics;
        
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

        Texture2D emissiveTx = ResourceDescriptorHeap[geoInfo.emissiveIdx];
        radiance += throughput * emissiveTx.SampleLevel(LinearWrapSampler, uv, 0).rgb;
        
       // Direct light evaluation
        //int lightIndex = 0;
        //float lightWeight = 0.0f;
        //if (SampleLightRIS(randSeed, worldPosition, worldNormal, lightIndex, lightWeight))
        //{
        //   // LightResult result = EvaluateLight(GetLight(lightIndex), hitLocation, V, N, geometryNormal, brdfData);
        //   // radiance += throughput * (result.Diffuse + result.Specular) * lightWeight;
        //}
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
void PT_ClosestHit(inout PT_Payload payload_data, in HitAttributes attribs)
{
    payload_data.isHit = true;
    payload_data.geoId = GeometryIndex();
    payload_data.triangleId = PrimitiveIndex();
    payload_data.barycentrics = attribs.barycentrics;
}


