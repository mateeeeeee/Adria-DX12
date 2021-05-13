#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

//https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-27-motion-blur-post-processing-effect

Texture2D scene_texture : register(t0);
Texture2D depth_texture : register(t1);

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
    float depth = depth_texture.Sample(linear_wrap_sampler, pin.Tex);
    float4 clip_space_position = GetClipSpacePosition(pin.Tex, depth);
    
    float4 world_pos = mul(clip_space_position, frame_cbuf.inverse_view_projection);
    
    world_pos /= world_pos.w;

    float4 prev_clip_space_position = mul(world_pos, frame_cbuf.prev_view_projection);
    
    prev_clip_space_position /= prev_clip_space_position.w;
    
    float2 velocity = (clip_space_position - prev_clip_space_position).xy / postprocess_cbuf.motion_blur_intensity;
    
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

//Handling Dynamic Objects
//This technique works perfectly for static objects because it only takes into account the movement of the camera. 
//However, if more accuracy is needed to record the velocity of dynamic objects in the scene, we can generate a separate velocity texture.
//To generate a velocity texture for rigid dynamic objects, transform the object by using the current frame's view-projection matrix and 
//the last frame's view-projection matrix, and then compute the difference in viewport positions the same way as for the post-processing pass.
// This velocity should be computed per-pixel by passing both transformed positions into the pixel shader and computing the velocity there. 
// This technique is described in the DirectX 9 SDK's motion blur sample (Microsoft 2006).