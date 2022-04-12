#include "../Globals/GlobalsRT.hlsli"
#include "../Util/RayTracingUtil.hlsli"


Texture2D depth_tx : register(t1);
RWTexture2D<float4> rtr_output : register(u0);

//move this to globalsrt.hlsli later
struct GeoInfo
{
    uint vertex_offset;
    uint index_offset;
    //add material indices later
};

struct Vertex
{
    float3 pos;
    float2 uv;
    float3 nor;
    float3 tan;
    float3 bin;
};

Texture2D Tex2DArray[] : register(t0, space1);

StructuredBuffer<Vertex> vertices : register(t0, space2);
StructuredBuffer<uint> indices : register(t1, space2);
StructuredBuffer<GeoInfo> geo_infos : register(t2, space2);

struct RTR_Payload
{
    float3 reflection_color;
    float  metallic;
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
    payload_data.metallic = 1.0f;
    payload_data.reflection_color = 0.0f;
    TraceRay(rt_scene,
		 RAY_FLAG_NONE,
		 0xFF, 0, 0, 0, ray, payload_data);

    rtr_output[launchIndex.xy].xyz = payload_data.metallic * payload_data.reflection_color;
    rtr_output[launchIndex.xy].w = 1.0f;

}

[shader("miss")]
void RTR_Miss(inout RTR_Payload payload_data)
{
    payload_data.reflection_color = float3(0, 0, 1); 
}


[shader("closesthit")]
void RTR_ClosestHit(inout RTR_Payload payload_data, in HitAttributes attribs)
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

    float3 barycentrics = float3(1.0f - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics);

    float3 v = barycentrics.x * v0.pos + barycentrics.y * v1.pos + barycentrics.z * v2.pos;
    
    payload_data.reflection_color = v;

}