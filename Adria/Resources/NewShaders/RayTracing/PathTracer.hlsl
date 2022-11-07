#include "../Constants.hlsli"
#include "../CommonResources.hlsli"
#include "RayTracingUtil.hlsli"

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
    float3 previousColor = accumTx[DispatchRaysIndex().xy].rgb;

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
        
        Texture2D albedoTx = ResourceDescriptorHeap[geoInfo.albedoIdx];
        radiance += throughput * albedoTx.SampleLevel(LinearWrapSampler, uv, 0).rgb;
        
        //radiance += float3(1, 0, 0);
        
        //Light light = light_cbuf.current_light;
    }

    // Accumulation and output
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


