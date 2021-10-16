#include "../Globals/GlobalsVS.hlsli"
#include "../Util/RootSignatures.hlsli"


struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 Uvs : TEX;
};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;
    float4 WorldPos     : POS;
    float2 TexCoord     : TEX;
};


SamplerState linear_wrap_sampler : register(s0);
Texture2D DisplacementMap : register(t1);



#define LAMBDA 1.2

[RootSignature(Ocean_RS)]
VS_OUTPUT main(VS_INPUT vin)
{
    VS_OUTPUT vout;
    
    float4 world_pos = mul(float4(vin.Pos, 1.0), object_cbuf.model);
    
    float3 wpos = world_pos.xyz / world_pos.w;
    
    float3 D_x = DisplacementMap.SampleLevel(linear_wrap_sampler, vin.Uvs, 0.0f).xyz;

    wpos += LAMBDA * D_x;
    
    world_pos = float4(wpos, 1.0f);
    
    vout.Position   = mul(world_pos, frame_cbuf.viewprojection);
    vout.TexCoord   = vin.Uvs; 
    vout.WorldPos   = world_pos;
    return vout;       
                       
}

/*
    
#define PERLIN 0

#if PERLIN 
Texture2D Perlin          : register(t3);

#define BLEND_START		800		
#define BLEND_END		20000		

static const float3 perlinFrequency = float3(1, 2, 4);
static const float3 perlinAmplitude = float3(0.8, 0.4, 0.2);
static const float3 perlinGradient  = float3(0.014, 0.016, 0.022);

#endif
    
#if PERLIN  //perlin noise
    
    float3 vdir = camera_pos - world_pos.xyz;
    float dist = length(vdir.xz);
    float factor = (BLEND_END - dist) / (BLEND_END - BLEND_START);
    float perl = 0.0f;

    factor = clamp(factor * factor * factor, 0.0, 1.0);

    float2 perlin_offset = float2(perlin_offset_x, perlin_offset_y);
    if (factor < 1.0)
    {
        float p0 = Perlin.SampleLevel(LinearWrapSampler, vin.Uvs * perlinFrequency.x + perlin_offset, 0.0f).a;
        float p1 = Perlin.SampleLevel(LinearWrapSampler, vin.Uvs * perlinFrequency.y + perlin_offset, 0.0f).a;
        float p2 = Perlin.SampleLevel(LinearWrapSampler, vin.Uvs * perlinFrequency.z + perlin_offset, 0.0f).a;

        perl = dot(float3(p0, p1, p2), perlinAmplitude);
    }
    
    D_x = lerp(float4(0.0, perl, 0.0, 1.0f), D_x, factor);
    
#endif

*/

