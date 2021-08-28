#include "../Globals/GlobalsRT.hlsli"

RaytracingAccelerationStructure rt_scene : register(t0);
Texture2D gbuf_pos : register(t1);
RWTexture2D<float> shadow_rt_output : register(u0);

struct ShadowRayData
{
    bool hit;
};

[shader("raygeneration")]
void RTS_RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint2 launchDim = DispatchRaysDimensions().xy;

    float3 posView = gbuf_pos.Load(int3(launchIndex.xy, 0)).xyz;
    float4 posWorld = mul(float4(posView, 1.0f), frame_cbuf.inverse_view);
    posWorld /= posWorld.w;

    Light light = light_cbuf.current_light;
    float3 direction;
    float maxT;

    if (light.type == POINT_LIGHT)
    {
        direction = light.position.xyz - posWorld.xyz;
        maxT = length(direction);
    }
    else if (light.type == DIRECTIONAL_LIGHT)
    {
        direction = -light.direction.xyz;
        maxT = 1000.0f;
    }
    else if (light.type == SPOT_LIGHT)
    {
        direction = -light.direction.xyz;
        maxT = length(light.position.xyz - posWorld.xyz);
    }

    RayDesc ray;
    ray.Origin = posWorld.xyz;
    ray.Direction = normalize(direction);
    ray.TMin = 0.01;
    ray.TMax = max(0.01, maxT);

    ShadowRayData payload;
    payload.hit = true;
    TraceRay(rt_scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 1, 0, ray, payload);
    shadow_rt_output[launchIndex.xy] = payload.hit ? 0.0f : 1.0f;
}

[shader("miss")]
void Rts_Miss(inout ShadowRayData hitData)
{
    hitData.hit = false;
}


[shader("anyhit")]
void Rts_Anyhit(inout ShadowRayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
    hitData.hit = true;
}








