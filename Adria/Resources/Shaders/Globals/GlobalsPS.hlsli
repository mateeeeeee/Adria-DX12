#include "../Util/CBuffers.hlsli"

ConstantBuffer<FrameCBuffer>        frame_cbuf          : register(b0);
ConstantBuffer<LightCBuffer>        light_cbuf          : register(b2);
ConstantBuffer<ShadowCBuffer>       shadow_cbuf         : register(b3);
ConstantBuffer<MaterialCBuffer>     material_cbuf       : register(b4);
ConstantBuffer<PostprocessCBuffer>  postprocess_cbuf    : register(b5);
ConstantBuffer<WeatherCBuffer>      weather_cbuf        : register(b7);

static const int SSAO_KERNEL_SIZE = 16;
static const int HBAO_NUM_STEPS = 4;
static const int HBAO_NUM_DIRECTIONS = 8;


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


float ExponentialFog(float dist)
{
    float fog_dist = max(dist - postprocess_cbuf.fog_start, 0.0);
    
    float fog = exp(-fog_dist * postprocess_cbuf.fog_density);
    return 1 - fog;
}

float ExponentialFog(float4 pos_vs)
{
    float4 pos_ws = mul(pos_vs, frame_cbuf.inverse_view);
    pos_ws /= pos_ws.w;
    float3 camera_to_world = (pos_ws - frame_cbuf.camera_position).xyz;

    float distance = length(camera_to_world);
    
    float fog_dist = max(distance - postprocess_cbuf.fog_start, 0.0);
    
    float fog = exp(-fog_dist * postprocess_cbuf.fog_density);
    return 1 - fog;
}

float ExponentialHeightFog(float4 pos_vs)
{
    float4 pos_ws = mul(pos_vs, frame_cbuf.inverse_view);
    pos_ws /= pos_ws.w;
    float3 camera_to_world = (pos_ws - frame_cbuf.camera_position).xyz;

    float distance = length(camera_to_world);
    
	// Find the fog staring distance to pixel distance
    float fogDist = max(distance - postprocess_cbuf.fog_start, 0.0);

	// Distance based fog intensity
    float fogHeightDensityAtViewer = exp(-postprocess_cbuf.fog_falloff * frame_cbuf.camera_position.y);
    float fogDistInt = fogDist * fogHeightDensityAtViewer;

	// Height based fog intensity
    float eyeToPixelY = camera_to_world.y * (fogDist / distance);
    float t = postprocess_cbuf.fog_falloff * eyeToPixelY;
    const float thresholdT = 0.01;
    float fogHeightInt = abs(t) > thresholdT ?
		(1.0 - exp(-t)) / t : 1.0;

	// Combine both factors to get the final factor
    float fog = exp(-postprocess_cbuf.fog_density * fogDistInt * fogHeightInt);

    return 1 - fog;
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

