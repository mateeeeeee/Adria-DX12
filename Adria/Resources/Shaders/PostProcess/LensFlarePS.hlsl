#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

struct VertextoPixel
{
    float4 pos : SV_POSITION;
    float3 texPos : TEXCOORD0;
    nointerpolation uint sel : TEXCOORD1;
    nointerpolation float4 opa : TEXCOORD2;
};


Texture2D lens0 : register(t0);
Texture2D lens1 : register(t1);
Texture2D lens2 : register(t2);
Texture2D lens3 : register(t3);
Texture2D lens4 : register(t4);
Texture2D lens5 : register(t5);
Texture2D lens6 : register(t6);
Texture2D depth_texture : register(t7);

SamplerState point_clamp_sampler : register(s0);


[RootSignature(LensFlare_RS)]
float4 main(VertextoPixel PSIn) : SV_TARGET
{
    float4 color = 0.0f;
	
	
	[branch]
    switch (PSIn.sel)
    {
        case 0:
            //color = lens0.SampleLevel(point_clamp_sampler, PSIn.texPos.xy, 0);
            color = float4(0.0f, 0.0f, 0.0f, 1.0f);
            break;
        case 1:
            color = lens1.SampleLevel(point_clamp_sampler, PSIn.texPos.xy, 0);
            break;
        case 2:
            color = lens2.SampleLevel(point_clamp_sampler, PSIn.texPos.xy, 0);
            break;
        case 3:
            color = lens3.SampleLevel(point_clamp_sampler, PSIn.texPos.xy, 0);
            break;
        case 4:
            color = lens4.SampleLevel(point_clamp_sampler, PSIn.texPos.xy, 0);
            break;
        case 5:
            color = lens5.SampleLevel(point_clamp_sampler, PSIn.texPos.xy, 0);
            break;
        case 6:
            color = lens6.SampleLevel(point_clamp_sampler, PSIn.texPos.xy, 0);
            break;
        default:
            break;
    };

    color *= 1.1 - saturate(PSIn.texPos.z);
    color *= PSIn.opa.x;

    return color;
}