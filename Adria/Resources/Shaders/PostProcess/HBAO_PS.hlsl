#include "../Globals/GlobalsPS.hlsli"


Texture2D normalTx : register(t1);
Texture2D<float> depthTx : register(t2);
Texture2D noiseTx : register(t3);

SamplerState linear_border_sampler : register(s0);
SamplerState point_wrap_sampler : register(s1);

static const float NdotVBias = 0.1f;

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


//----------------------------------------------------------------------------------
float Falloff(float DistanceSquare)
{
  // 1 scalar mad instruction
    return DistanceSquare * (-1.0f / postprocess_cbuf.hbao_r2) + 1.0;
}

//----------------------------------------------------------------------------------
// P = view-space position at the kernel center
// N = view-space normal at the kernel center
// S = view-space position of the current sample
//----------------------------------------------------------------------------------
float ComputeAO(float3 P, float3 N, float3 S)
{
    float3 V = S - P;
    float VdotV = dot(V, V);
    float NdotV = dot(N, V) * 1.0 / sqrt(VdotV);

  // Use saturate(x) instead of max(x,0.f) because that is faster on Kepler
    return clamp(NdotV - NdotVBias, 0, 1) * clamp(Falloff(VdotV), 0, 1);
}

//----------------------------------------------------------------------------------
float2 RotateDirection(float2 Dir, float2 CosSin)
{
    return float2(Dir.x * CosSin.x - Dir.y * CosSin.y,
              Dir.x * CosSin.y + Dir.y * CosSin.x);
}

//----------------------------------------------------------------------------------
float ComputeCoarseAO(float2 UV, float radius_in_pixels, float3 rand, float3 pos_vs, float3 normal_vs)
{
  // Divide by NUM_STEPS+1 so that the farthest samples are not fully attenuated
    float step_size_in_pixels = radius_in_pixels / (HBAO_NUM_STEPS + 1);

    const float theta = 2.0 * PI / HBAO_NUM_DIRECTIONS;
    float AO = 0;

    for (float direction_index = 0; direction_index < HBAO_NUM_DIRECTIONS; ++direction_index)
    {
        float angle = theta * direction_index;

        // Compute normalized 2D direction
        float2 direction = RotateDirection(float2(cos(angle), sin(angle)), rand.xy);

        // Jitter starting sample within the first step
        float ray_t = (rand.z * step_size_in_pixels + 1.0);

        for (float step_index = 0; step_index < HBAO_NUM_STEPS; ++step_index)
        {
            float2 SnappedUV = round(ray_t * direction) / frame_cbuf.screen_resolution + UV;
           
            float depth = depthTx.Sample(linear_border_sampler, SnappedUV);
            float3 S = GetPositionVS(SnappedUV, depth);
            
            ray_t += step_size_in_pixels;

            AO += ComputeAO(pos_vs, normal_vs, S);
        }
    }

    AO *= (1.0f / (1.0f - NdotVBias)) / (HBAO_NUM_DIRECTIONS * HBAO_NUM_STEPS);
    return clamp(1.0 - AO * 2.0, 0, 1);
}

float main(VertexOut pin) : SV_TARGET
{
    const float TWO_PI = 2.0 * PI;
    
    float depth = depthTx.Sample(linear_border_sampler, pin.Tex);
    
    float3 pos_vs = GetPositionVS(pin.Tex, depth);

    float3 normal_vs_encoded = normalTx.Sample(linear_border_sampler, pin.Tex).rgb; //zamijeni kasnije sa pointSamplerom
    float3 normal_vs = 2 * normal_vs_encoded - 1.0;
    normal_vs = normalize(normal_vs);
   
    // Compute projection of disk of radius control.R into screen space
    float radius_in_pixels = postprocess_cbuf.hbao_radius_to_screen / pos_vs.z;
    
    float3 rand = noiseTx.Sample(point_wrap_sampler, pin.Tex * postprocess_cbuf.noise_scale).xyz; //use wrap sampler

    float AO = ComputeCoarseAO(pin.Tex, radius_in_pixels, rand, pos_vs, normal_vs);
    
    return pow(AO, postprocess_cbuf.hbao_power);

}