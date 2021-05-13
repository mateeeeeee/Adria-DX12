
#include "../Util/ToneMapUtil.hlsli"
#include "../Globals/GlobalsPS.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};

Texture2D hdr_texture : register(t0);
SamplerState linear_wrap_sampler : register(s0);

#include "../Util/RootSignatures.hlsli"

[RootSignature(ToneMap_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
    
    float4 color = hdr_texture.Sample(linear_wrap_sampler, pin.Tex);
    float4 tone_mapped_color = 0.0f;
    
    switch (postprocess_cbuf.tone_map_op)
    {
        case 0:
            tone_mapped_color = float4(ReinhardToneMapping(color.xyz * postprocess_cbuf.tone_map_exposure), 1.0);
            break;
        case 1:
            tone_mapped_color = float4(HableToneMapping(color.xyz * postprocess_cbuf.tone_map_exposure), 1.0);
            break;
        case 2:
            tone_mapped_color = float4(LinearToneMapping(color.xyz * postprocess_cbuf.tone_map_exposure), 1.0);
            break;
        default:
            tone_mapped_color = float4(color.xyz * postprocess_cbuf.tone_map_exposure, 1.0f);
    }

    return tone_mapped_color;

}