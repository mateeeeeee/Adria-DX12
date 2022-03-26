#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

Texture2D<float4> txAlbedoDecal : register(t0);
Texture2D<float4> txNormalDecal : register(t1);
Texture2D<float>  txDepth       : register(t2);

SamplerState point_clamp_sampler : register(s0);
SamplerState linear_wrap_sampler : register(s1);


struct PS_DECAL_OUT
{
    float4 DiffuseRoughness : SV_TARGET0;
#ifdef DECAL_MODIFY_NORMALS
    float4 NormalMetallic   : SV_TARGET1;
#endif
};

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float4 ClipSpacePos : POSITION;
    matrix InverseModel : INVERSE_MODEL;
};

#define DECAL_WALL  0
#define DECAL_FLOOR 1

cbuffer DecalCBuffer : register(b11)
{
    int decal_type;
}

[RootSignature(Decal_RS)]
PS_DECAL_OUT main(PS_INPUT input)
{
    PS_DECAL_OUT pout = (PS_DECAL_OUT) 0;

    float2 screen_pos = input.ClipSpacePos.xy / input.ClipSpacePos.w;
    float2 depth_coords = screen_pos * float2(0.5f, -0.5f) + 0.5f;
    float depth = txDepth.Sample(point_clamp_sampler, depth_coords).r;

    float4 posVS = float4(GetPositionVS(depth_coords, depth), 1.0f);
    float4 posWS = mul(posVS, frame_cbuf.inverse_view);
    float4 posLS = mul(posWS, input.InverseModel);
    posLS.xyz /= posLS.w;

    clip(0.5f - abs(posLS.xyz));

    float2 tex_coords = 0.0f;
    switch (decal_type)
    {
        case DECAL_WALL:
            tex_coords = posLS.xy + 0.5f;
            break;
        case DECAL_FLOOR:
            tex_coords = posLS.xz + 0.5f;
            break;
        default:
            pout.DiffuseRoughness.rgb = float3(1, 0, 0);
            return pout;
    }

    float4 albedo = txAlbedoDecal.SampleLevel(linear_wrap_sampler, tex_coords, 0);
    if (albedo.a < 0.1)
        discard;
    pout.DiffuseRoughness.rgb = albedo.rgb;

#ifdef DECAL_MODIFY_NORMALS
    posWS /= posWS.w;
    float3 ddx_ws = ddx(posWS.xyz);
    float3 ddy_ws = ddy(posWS.xyz);

    float3 normal   = normalize(cross(ddx_ws, ddy_ws));
    float3 binormal = normalize(ddx_ws);
    float3 tangent  = normalize(ddy_ws);

    float3x3 TBN = float3x3(tangent, binormal, normal);

    float3 DecalNormal = txNormalDecal.Sample(linear_wrap_sampler, tex_coords);
    DecalNormal = 2.0f * DecalNormal - 1.0f;
    DecalNormal = mul(DecalNormal, TBN);
    float3 DecalNormalVS = normalize(mul(DecalNormal, (float3x3)frame_cbuf.view));
    pout.NormalMetallic.rgb = 0.5 * DecalNormalVS + 0.5;
#endif

    return pout;
}