#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

//https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-27-motion-blur-post-processing-effect

Texture2D scene_texture : register(t0);
Texture2D<float2> velocity_buffer : register(t1);

SamplerState linear_wrap_sampler : register(s0);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};

static const int SAMPLE_COUNT = 16;

[RootSignature(MotionBlur_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
    float2 velocity = velocity_buffer.SampleLevel(linear_wrap_sampler, pin.Tex, 0) / postprocess_cbuf.motion_blur_intensity;
    
    float2 tex_coord = pin.Tex;

    float4 color = scene_texture.Sample(linear_wrap_sampler, tex_coord);
    
    tex_coord += velocity;
    
    for (int i = 1; i < SAMPLE_COUNT; ++i)
    {
       
        float4 currentColor = scene_texture.Sample(linear_wrap_sampler, tex_coord);
   
    	 color += currentColor;
        
        tex_coord += velocity;
    }
    
    float4 final_color = color / SAMPLE_COUNT;
    
    return final_color;

}
