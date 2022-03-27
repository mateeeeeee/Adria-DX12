#include "../Util/CBuffers.hlsli"

ConstantBuffer<FrameCBuffer> frame_cbuf             : register(b0);
ConstantBuffer<LightCBuffer> light_cbuf             : register(b2);
ConstantBuffer<RayTracingCBuffer> ray_tracing_cbuf  : register(b10);

float3 GetPositionVS(float2 texcoord, float depth)
{
    float4 clipSpaceLocation;
    clipSpaceLocation.xy = texcoord * 2.0f - 1.0f;
    clipSpaceLocation.y *= -1;
    clipSpaceLocation.z = depth;
    clipSpaceLocation.w = 1.0f;
    float4 homogenousLocation = mul(clipSpaceLocation, frame_cbuf.inverse_projection);
    return homogenousLocation.xyz / homogenousLocation.w;
}

inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5;
    float2 screenPos = xy / frame_cbuf.screen_resolution * 2.0 - 1.0;
    screenPos.y = -screenPos.y;
    float4 unprojected = mul(float4(screenPos, 0, 1), frame_cbuf.inverse_view);
    float3 world = unprojected.xyz / unprojected.w;
    origin = frame_cbuf.camera_position;
    direction = normalize(world - origin);
}