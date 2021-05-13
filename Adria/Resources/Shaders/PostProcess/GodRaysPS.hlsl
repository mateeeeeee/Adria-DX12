#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

Texture2D scene_texture : register(t0);

SamplerState linear_clamp_sampler : register(s0);


static const int NUM_SAMPLES = 32;

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


[RootSignature(GodRays_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
    float2 texCoord = pin.Tex;
// Calculate vector from pixel to light source in screen space.
    float2 deltaTexCoord = (texCoord - light_cbuf.current_light.ss_position.xy);
// Divide by number of samples and scale by control factor.
    deltaTexCoord *= light_cbuf.current_light.godrays_density / NUM_SAMPLES;
// Store initial sample.
    
// Set up illumination decay factor.
    float illuminationDecay = 1.0f;
// Evaluate summation from Equation 3 NUM_SAMPLES iterations.
    
    float3 accumulated_god_rays = float3(0.0f,0.0f,0.0f);
    
    float3 color = scene_texture.SampleLevel(linear_clamp_sampler, texCoord, 0).rgb;
    
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
// Step sample location along ray.
        texCoord -= deltaTexCoord;
// Retrieve sample at new location.
        float4 sample = scene_texture.SampleLevel(linear_clamp_sampler, texCoord, 0);
// Apply sample attenuation scale/decay factors.
        sample *= illuminationDecay * light_cbuf.current_light.godrays_weight;
// Accumulate combined color.
        accumulated_god_rays += (sample * light_cbuf.current_light.color).xyz;
// Update exponential decay factor.
        illuminationDecay *= light_cbuf.current_light.godrays_decay;
    }
    
    accumulated_god_rays = max(accumulated_god_rays, float3(0.0f, 0.0f, 0.0f));
    
// Output final color with a further scale control factor.
    return float4(color, 1.0f) + float4(accumulated_god_rays, 1.0f) * light_cbuf.current_light.godrays_exposure; //
}
