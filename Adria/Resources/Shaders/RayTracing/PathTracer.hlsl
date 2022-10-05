#include "../Globals/GlobalsRT.hlsli"
#include "../Util/RayTracingUtil.hlsli"

RWTexture2D<float4> pt_output : register(u0);
RWTexture2D<float4> pt_accumulation : register(u1);

TextureCube env_map : register(t2);
Texture2D Tex2DArray[] : register(t0, space1);

StructuredBuffer<Vertex> vertices : register(t0, space2);
StructuredBuffer<uint> indices : register(t1, space2);
StructuredBuffer<GeoInfo> geo_infos : register(t2, space2);
SamplerState linear_wrap_sampler : register(s0);

struct PT_Payload
{
    bool is_hit;
    uint geo_id;
    uint triangle_id;
    float2 barycentrics;
};

[shader("raygeneration")]
void PT_RayGen()
{
    float2 pixel = float2(DispatchRaysIndex().xy);
    float2 resolution = float2(DispatchRaysDimensions().xy);
    
    uint randSeed = InitRand(pixel.x + pixel.y * resolution.x, ray_tracing_cbuf.frame_count, 16);
    float3 previousColor = pt_accumulation[DispatchRaysIndex().xy].rgb;

    // Jitter to achieve anti-aliasing
    float2 offset = float2(NextRand(randSeed), NextRand(randSeed));
    pixel += lerp(-0.5f.xx, 0.5f.xx, offset);
    
    float2 ncdXY = (pixel / (resolution * 0.5f)) - 1.0f;
    ncdXY.y *= -1.0f;
    float4 rayStart = mul(float4(ncdXY, 0.0f, 1.0f), frame_cbuf.inverse_view_projection);
    float4 rayEnd = mul(float4(ncdXY, 1.0f, 1.0f), frame_cbuf.inverse_view_projection);

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz /= rayEnd.w;
    float3 rayDir = normalize(rayEnd.xyz - rayStart.xyz);
    float rayLength = length(rayEnd.xyz - rayStart.xyz);
    
    float3 o, d;
    GenerateCameraRay(pixel, o, d);
    RayDesc ray;
    ray.Origin = rayStart;
    ray.Direction = rayDir;
    ray.TMin = 0.0f;
    ray.TMax = FLT_MAX;

    float3 radiance = 0.0f;
    float3 throughput = 1.0f; 
    for (int i = 0; i < 1; ++i) //ray_tracing_cbuf.bounce_count
    {
        PT_Payload payload_data;
        TraceRay(rt_scene,
		 RAY_FLAG_FORCE_OPAQUE,
		 0xFF, 0, 0, 0, ray, payload_data);
        
        if (!payload_data.is_hit)
        {
            radiance += throughput * env_map.SampleLevel(linear_wrap_sampler, ray.Direction, 0).rgb;
            break;
        }

        uint geo_id = payload_data.geo_id;
        uint triangle_id = payload_data.triangle_id;
        float2 barycentrics = payload_data.barycentrics;
        
        GeoInfo geo_info = geo_infos[geo_id];
        uint vb_offset = geo_info.vertex_offset;
        uint ib_offset = geo_info.index_offset;
    
        uint i0 = indices[ib_offset + triangle_id * 3 + 0];
        uint i1 = indices[ib_offset + triangle_id * 3 + 1];
        uint i2 = indices[ib_offset + triangle_id * 3 + 2];
    
        Vertex v0 = vertices[vb_offset + i0];
        Vertex v1 = vertices[vb_offset + i1];
        Vertex v2 = vertices[vb_offset + i2];

        float3 pos = Interpolate(v0.pos, v1.pos, v2.pos, barycentrics);
        float2 uv = Interpolate(v0.uv, v1.uv, v2.uv, barycentrics);
        uv.y = 1.0f - uv.y;
        float3 nor = normalize(Interpolate(v0.nor, v1.nor, v2.nor, barycentrics));
        float3 pos_ws = mul(pos, ObjectToWorld3x4()).xyz;
        float3 nor_ws = mul(nor, (float3x3) WorldToObject4x3());

        Texture2D txEmissive = Tex2DArray[geo_info.emissive_idx];
        radiance += throughput * txEmissive.SampleLevel(linear_wrap_sampler, uv, 0).rgb;
        
        Texture2D txAlbedo = Tex2DArray[geo_info.albedo_idx];
        radiance += throughput * txAlbedo.SampleLevel(linear_wrap_sampler, uv, 0).rgb;
        
        //radiance += float3(1, 0, 0);
        
        Light light = light_cbuf.current_light;
    }

    //Accumulation and output
    if (ray_tracing_cbuf.accumulated_frames > 1)
    {
        radiance += previousColor;
    }
    pt_accumulation[DispatchRaysIndex().xy] = float4(radiance, 1.0f);
    pt_output[DispatchRaysIndex().xy] = float4(radiance, 1.0f);// / ray_tracing_cbuf.accumulated_frames;
}

[shader("miss")]
void PT_Miss(inout PT_Payload payload_data)
{
    payload_data.is_hit = false;
}


[shader("closesthit")]
void PT_ClosestHit(inout PT_Payload payload_data, in HitAttributes attribs)
{
    payload_data.is_hit = true;
    payload_data.geo_id = GeometryIndex();
    payload_data.triangle_id = PrimitiveIndex();
    payload_data.barycentrics = attribs.barycentrics;
}








