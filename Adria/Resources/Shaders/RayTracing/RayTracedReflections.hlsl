#include "../Globals/GlobalsRT.hlsli"
#include "../Util/RayTracingUtil.hlsli"


Texture2D depth_tx : register(t1);
TextureCube env_map : register(t2);

RWTexture2D<float4> rtr_output : register(u0);

//move this to globalsrt.hlsli later
struct GeoInfo
{
    uint vertex_offset;
    uint index_offset;
    
    int albedo_idx;
    int normal_idx;
    int metallic_roughness_idx;
    int emissive_idx;
};

struct Vertex
{
    float3 pos;
    float2 uv;
    float3 nor;
    float3 tan;
    float3 bin;
};


static float3 Interpolate(in float3 x0, in float3 x1, in float3 x2, float2 bary)
{
    return x0 * (1.0f - bary.x - bary.y) + bary.x * x1 + bary.y * x2;
}

static float2 Interpolate(in float2 x0, in float2 x1, in float2 x2, float2 bary)
{
    return x0 * (1.0f - bary.x - bary.y) + bary.x * x1 + bary.y * x2;
}

Texture2D Tex2DArray[] : register(t0, space1);

StructuredBuffer<Vertex> vertices : register(t0, space2);
StructuredBuffer<uint> indices : register(t1, space2);
StructuredBuffer<GeoInfo> geo_infos : register(t2, space2);
SamplerState linear_wrap_sampler : register(s0);

struct RTR_Payload
{
    float3 reflection_color;
    float  reflectivity;
};

[shader("raygeneration")]
void RTR_RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float depth = depth_tx.Load(int3(launchIndex.xy, 0)).r;
    float2 tex_coords = (launchIndex + 0.5f) / frame_cbuf.screen_resolution;
    float3 posView = GetPositionVS(tex_coords, depth);
    float4 posWorld = mul(float4(posView, 1.0f), frame_cbuf.inverse_view);
    posWorld /= posWorld.w;
    
    RayDesc ray;
    ray.Origin = frame_cbuf.camera_position.xyz;
    ray.Direction = normalize(posWorld.xyz - frame_cbuf.camera_position.xyz);
    ray.TMin = 0.005f;
    ray.TMax = FLT_MAX;
        
    RTR_Payload payload_data;
    payload_data.reflectivity = 0.0f;
    payload_data.reflection_color = 0.0f;
    TraceRay(rt_scene,
		 RAY_FLAG_FORCE_OPAQUE,
		 0xFF, 0, 0, 0, ray, payload_data);

        
    rtr_output[launchIndex.xy] = float4(payload_data.reflectivity * payload_data.reflection_color, 1.0f);
}

[shader("miss")]
void RTR_Miss(inout RTR_Payload payload_data)
{
    payload_data.reflection_color = env_map.SampleLevel(linear_wrap_sampler, WorldRayDirection(), 0);
}


[shader("closesthit")]
void RTR_ClosestHitPrimaryRay(inout RTR_Payload payload_data, in HitAttributes attribs)
{
    uint geo_id = GeometryIndex();
    uint triangle_id = PrimitiveIndex();
    
    GeoInfo geo_info = geo_infos[geo_id];
    uint vb_offset = geo_info.vertex_offset;
    uint ib_offset = geo_info.index_offset;
    
    uint i0 = indices[ib_offset + triangle_id * 3 + 0];
    uint i1 = indices[ib_offset + triangle_id * 3 + 1];
    uint i2 = indices[ib_offset + triangle_id * 3 + 2];
    
    Vertex v0 = vertices[vb_offset + i0];
    Vertex v1 = vertices[vb_offset + i1];
    Vertex v2 = vertices[vb_offset + i2];

    float3 pos = Interpolate(v0.pos, v1.pos, v2.pos, attribs.barycentrics);
    float2 uv = Interpolate(v0.uv, v1.uv, v2.uv, attribs.barycentrics); uv.y = 1.0f - uv.y;
    float3 nor = normalize(Interpolate(v0.nor, v1.nor, v2.nor, attribs.barycentrics));
    float3 pos_ws = mul(pos, ObjectToWorld3x4()).xyz;
    float3 nor_ws = mul(nor, (float3x3) WorldToObject4x3());
    
    Texture2D txMetallicRoughness = Tex2DArray[geo_info.metallic_roughness_idx];
    float2 roughness_metallic = txMetallicRoughness.SampleLevel(linear_wrap_sampler, uv, 0).gb;
    
    if (roughness_metallic.y <= 0.01f) return;

    RayDesc reflection_ray;
    reflection_ray.Origin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection(); //OffsetRay(pos_ws, nor_ws);
    reflection_ray.Direction = reflect(WorldRayDirection(), nor_ws);
    reflection_ray.TMin = 0.01f;
    reflection_ray.TMax = FLT_MAX;
    
    TraceRay(rt_scene,
		 RAY_FLAG_FORCE_OPAQUE,
		 0xFF, 1, 0, 0, reflection_ray, payload_data);
    
    payload_data.reflectivity = roughness_metallic.y * (1.0f - roughness_metallic.x) * (pos.y < 2.0f);
}

[shader("closesthit")]
void RTR_ClosestHitReflectionRay(inout RTR_Payload payload_data, in HitAttributes attribs)
{
    uint geo_id = GeometryIndex();
    uint triangle_id = PrimitiveIndex();
    
    GeoInfo geo_info = geo_infos[geo_id];
    uint vb_offset = geo_info.vertex_offset;
    uint ib_offset = geo_info.index_offset;
    
    uint i0 = indices[ib_offset + triangle_id * 3 + 0];
    uint i1 = indices[ib_offset + triangle_id * 3 + 1];
    uint i2 = indices[ib_offset + triangle_id * 3 + 2];
    
    Vertex v0 = vertices[vb_offset + i0];
    Vertex v1 = vertices[vb_offset + i1];
    Vertex v2 = vertices[vb_offset + i2];
    
    float2 uv = Interpolate(v0.uv, v1.uv, v2.uv, attribs.barycentrics); uv.y = 1.0f - uv.y;
    Texture2D txAlbedo = Tex2DArray[geo_info.albedo_idx];
    float3 albedo = txAlbedo.SampleLevel(linear_wrap_sampler, uv, 0).rgb;
    payload_data.reflection_color = albedo;
}
