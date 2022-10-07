#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

Texture2D<float> depth_texture : register(t0);

SamplerState linear_wrap_sampler : register(s0);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};

[RootSignature(VelocityBuffer_RS)]
float2 main(VertexOut pin) : SV_Target0
{
    float depth = depth_texture.Sample(linear_wrap_sampler, pin.Tex);
    float4 clip_space_position = GetClipSpacePosition(pin.Tex, depth);
    
    float4 world_pos = mul(clip_space_position, frame_cbuf.inverse_view_projection);
    
    world_pos /= world_pos.w;
    
    float4 prev_clip_space_position = mul(world_pos, frame_cbuf.prev_view_projection);
    
    prev_clip_space_position /= prev_clip_space_position.w;
    
    float2 velocity = (clip_space_position - prev_clip_space_position).xy / postprocess_cbuf.velocity_buffer_scale;
    
    return velocity;
}