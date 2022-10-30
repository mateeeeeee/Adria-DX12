#include "../Util/CBuffers.hlsli"

ConstantBuffer<FrameCBuffer>    frame_cbuf   : register(b0);
ConstantBuffer<ComputeCBuffer> compute_cbuf : register(b6);

float ConvertZToLinearDepth(float depth)
{
    float near = frame_cbuf.camera_near;
    float far = frame_cbuf.camera_far;
    
    return (near * far) / (far - depth * (far - near));
}

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