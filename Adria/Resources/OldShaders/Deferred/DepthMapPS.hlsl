

#if TRANSPARENT
SamplerState linear_wrap_sampler : register(s0);
Texture2D albedo_texture : register(t0);
#endif

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
#if TRANSPARENT
    float2 TexCoords : TEX;
#endif
};

#include "../Util/RootSignatures.hlsli"
#if TRANSPARENT
[RootSignature(DepthMapTransparent_RS)]
#else 
[RootSignature(DepthMap_RS)]
#endif
void main(VS_OUTPUT pin)//: SV_Target
{
#if TRANSPARENT 
    if( albedo_texture.Sample(linear_wrap_sampler,pin.TexCoords).a < 0.1 ) 
        discard;
#endif
}