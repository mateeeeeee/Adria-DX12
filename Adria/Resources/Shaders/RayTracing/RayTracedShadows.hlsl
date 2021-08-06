
#include "../Util/RayTracingUtils.hlsli"

RaytracingAccelerationStructure scene : register(t0);
RWTexture2D<float> shadowOutput : register(u0);


[shader("closesthit")]
void ShadowHitShader(inout ShadowPayload payload, in HitAttributes attr)
{
    payload.Visibility = 0.0f;
}

[shader("miss")]
void ShadowMissShader(inout ShadowPayload payload)
{
    payload.Visibility = 1.0f;
}






