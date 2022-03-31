#include "../Globals/GlobalsRT.hlsli"
#include "../Util/RayTracingUtil.hlsli"

RaytracingAccelerationStructure rt_scene : register(t0);
Texture2D depth_tx : register(t1);
Texture2D gbuf_nor : register(t2);
RWTexture2D<float> ao_rt_output : register(u0);


struct AORayData
{
    bool hit;
};





static const int RAY_COUNT = 16;

[shader("raygeneration")]
void RTAO_RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float depth = depth_tx.Load(int3(launchIndex.xy, 0)).r;
    float2 tex_coords = (launchIndex + 0.5f) / frame_cbuf.screen_resolution;

    float3 posView = GetPositionVS(tex_coords, depth);
    float4 posWorld = mul(float4(posView, 1.0f), frame_cbuf.inverse_view);
    posWorld /= posWorld.w;
    
    float3 normalView = gbuf_nor.Load(int3(launchIndex.xy, 0)).xyz;
    normalView = 2 * normalView - 1.0;
    float3 normalWorld = normalize(mul(normalView, (float3x3) transpose(frame_cbuf.view)));

    uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, ray_tracing_cbuf.frame_count, 16);

    float ao = 0.0f;
    [unroll(RAY_COUNT)]
    for (int i = 0; i < RAY_COUNT; i++)
    {
        float3 worldDir = GetCosHemisphereSample(randSeed, normalWorld.xyz);
        AORayData rayPayload = { true };
        RayDesc rayAO;
        rayAO.Origin = OffsetRay(posWorld.xyz, normalWorld.xyz);
        rayAO.Direction = normalize(worldDir);
        rayAO.TMin = 0.05f;
        rayAO.TMax = ray_tracing_cbuf.rtao_radius;

        TraceRay(rt_scene,
		 RAY_FLAG_NONE,
		 0xFF, 0, 1, 0, rayAO, rayPayload);
        ao += rayPayload.hit ? 0.0f : 1.0f;
    }

    ao_rt_output[launchIndex.xy] = ao / RAY_COUNT;
}

[shader("miss")]
void RTAO_Miss(inout AORayData hitData)
{
    hitData.hit = false;
}

[shader("anyhit")]
void RTAO_AnyHit(inout AORayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
    //hitData.hit = true;
}