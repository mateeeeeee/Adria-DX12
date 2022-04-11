#include "../Globals/GlobalsRT.hlsli"
#include "../Util/RayTracingUtil.hlsli"

RaytracingAccelerationStructure rt_scene : register(t0);
Texture2D depth_tx : register(t1);
Texture2D gbuf_nor : register(t2);
RWTexture2D<float> ao_rt_output : register(u0);


struct RTR_Payload
{
    
};

[shader("raygeneration")]
void RTR_RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

}

[shader("miss")]
void RTR_Miss(inout RTR_Payload hitData)
{
   
}

[shader("anyhit")]
void RTR_AnyHit(inout RTR_Payload hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
    
}

[shader("closesthit")]
void RTR_ClosestHit(inout RTR_Payload hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
    
}