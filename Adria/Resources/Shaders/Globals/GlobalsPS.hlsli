#include "../Util/CBuffers.hlsli"

ConstantBuffer<FrameCBuffer>        frame_cbuf          : register(b0);
ConstantBuffer<LightCBuffer>        light_cbuf          : register(b2);
ConstantBuffer<ShadowCBuffer>       shadow_cbuf         : register(b3);
ConstantBuffer<MaterialCBuffer>     material_cbuf       : register(b4);
ConstantBuffer<PostprocessCBuffer>  postprocess_cbuf    : register(b5);
ConstantBuffer<WeatherCBuffer>      weather_cbuf        : register(b7);
static const int SSAO_KERNEL_SIZE = 16;


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

float ConvertZToLinearDepth(float depth)
{
    float near = frame_cbuf.camera_near;
    float far = frame_cbuf.camera_far;
    
    return (near * far) / (far - depth * (far - near));

}


float4 GetClipSpacePosition(float2 texcoord, float depth)
{
    float4 clipSpaceLocation;
    clipSpaceLocation.xy = texcoord * 2.0f - 1.0f;
    clipSpaceLocation.y *= -1;
    clipSpaceLocation.z = depth;
    clipSpaceLocation.w = 1.0f;
    
    return clipSpaceLocation;
}


float GetFogAmount(float dist)
{
    return saturate((dist - frame_cbuf.fog_near) / (frame_cbuf.fog_far - frame_cbuf.fog_near));
}


bool IsSaturated(float value)
{
    return value == saturate(value);
}
bool IsSaturated(float2 value)
{
    return IsSaturated(value.x) && IsSaturated(value.y);
}
bool IsSaturated(float3 value)
{
    return IsSaturated(value.x) && IsSaturated(value.y) && IsSaturated(value.z);
}
bool IsSaturated(float4 value)
{
    return IsSaturated(value.x) && IsSaturated(value.y) && IsSaturated(value.z) && IsSaturated(value.w);
}
float ScreenFade(float2 uv)
{
    float2 fade = max(12.0f * abs(uv - 0.5f) - 5.0f, 0.0f);
    return saturate(1.0 - dot(fade, fade));
}

