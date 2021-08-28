#include "../Globals/GlobalsRT.hlsli"
#include "../Util/RayTracingUtil.hlsli"

RaytracingAccelerationStructure rt_scene : register(t0);
Texture2D gbuf_pos : register(t1);
Texture2D gbuf_nor : register(t2);
RWTexture2D<float> ao_rt_output : register(u0);


struct AORayData
{
    bool hit;
};

[shader("raygeneration")]
void RTAO_RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint2 launchDim = DispatchRaysDimensions().xy;

    //change from view to world
    float3 posView = gbuf_pos.Load(int3(launchIndex.xy, 0)).xyz;
    float3 normalView = gbuf_nor.Load(int3(launchIndex.xy, 0)).xyz;
    
    float4 posWorld = mul(float4(posView, 1.0f), frame_cbuf.inverse_view);
    float3 normalWorld = mul(normalView, (float3x3) transpose(frame_cbuf.view));

    uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, ray_tracing_cbuf.frame_count, 16);
    float3 directionWorld = GetCosHemisphereSample(randSeed, normalWorld.xyz);

    RayDesc ray;
    ray.Origin = posWorld.xyz;
    ray.Direction = normalize(directionWorld);
    ray.TMin = 0.01;
    ray.TMax = max(0.01, ray_tracing_cbuf.rtao_radius);

    AORayData payload;
    payload.hit = true;

    TraceRay(rt_scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 1, 0, ray, payload);
    ao_rt_output[launchIndex.xy] = payload.hit ? 0.0f : 1.0f;
}

[shader("miss")]
void RTAO_Miss(inout AORayData hitData)
{
    hitData.hit = false;
}

[shader("anyhit")]
void RTAO_AnyHit(inout AORayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
    hitData.hit = true;
}