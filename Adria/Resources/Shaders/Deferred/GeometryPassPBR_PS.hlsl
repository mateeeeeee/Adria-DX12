#include "../Globals/GlobalsPS.hlsli"


SamplerState linear_wrap_sampler : register(s0);

Texture2D txAlbedo              : register(t0);

Texture2D txNormal              : register(t2);

Texture2D txEmissive            : register(t3);

#if !METALLIC_ROUGHNESS_SEPARATED

Texture2D txMetallicRoughness   : register(t1);

#else 

Texture2D<float> txMetallic            : register(t1);

Texture2D<float> txRoughness           : register(t4);

#endif 



struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 Uvs : TEX;
    float3 NormalVS : NORMAL0;
    float3 TangentWS : TANGENT;
    float3 BitangentWS : BITANGENT;
    float3 NormalWS : NORMAL1;
};


struct PS_GBUFFER_OUT
{
    float4 NormalMetallic : SV_TARGET0;
    float4 DiffuseRoughness : SV_TARGET1;
    float4 Emissive : SV_TARGET2;
};


PS_GBUFFER_OUT PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic)
{
    PS_GBUFFER_OUT Out;

    Out.NormalMetallic = float4(0.5 * NormalVS + 0.5, metallic);
    Out.DiffuseRoughness = float4(BaseColor, roughness);
    Out.Emissive = float4(emissive.rgb, emissive.a / 256);
    return Out;
}



#include "../Util/RootSignatures.hlsli"

#if !METALLIC_ROUGHNESS_SEPARATED
[RootSignature(GeometryPassPBR_RS)]
#else 
[RootSignature(GeometryPassPBR_Separated_RS)]
#endif
PS_GBUFFER_OUT main(VS_OUTPUT In)
{

    In.Uvs.y = 1 - In.Uvs.y;
 
    float4 DiffuseColor = txAlbedo.Sample(linear_wrap_sampler, In.Uvs) * material_cbuf.albedo_factor;

    if (DiffuseColor.a < 0.1)
        discard;

    float3 Normal = normalize(In.NormalWS);
    float3 Tangent = normalize(In.TangentWS);
    float3 Bitangent = normalize(In.BitangentWS);
    float3 BumpMapNormal = txNormal.Sample(linear_wrap_sampler, In.Uvs).xyz;
    BumpMapNormal = 2.0f * BumpMapNormal - 1.0f;
    float3x3 TBN = float3x3(Tangent, Bitangent, Normal);
    float3 NewNormal = mul(BumpMapNormal, TBN);
    In.NormalVS = normalize(mul(NewNormal, (float3x3) frame_cbuf.view));

#if !METALLIC_ROUGHNESS_SEPARATED
    float3 ao_roughness_metallic = txMetallicRoughness.Sample(linear_wrap_sampler, In.Uvs).rgb;

    float3 EmissiveColor = txEmissive.Sample(linear_wrap_sampler, In.Uvs).rgb;
    return PackGBuffer(DiffuseColor.xyz, normalize(In.NormalVS), float4(EmissiveColor, material_cbuf.emissive_factor),
    ao_roughness_metallic.g * material_cbuf.roughness_factor, ao_roughness_metallic.b * material_cbuf.metallic_factor); 
#else 
    float metallic = txMetallic.Sample(linear_wrap_sampler, In.Uvs).r;
    float roughness = txRoughness.Sample(linear_wrap_sampler, In.Uvs).r;

    float3 EmissiveColor = txEmissive.Sample(linear_wrap_sampler, In.Uvs).rgb;
    return PackGBuffer(DiffuseColor.xyz, normalize(In.NormalVS), float4(EmissiveColor, material_cbuf.emissive_factor),
    roughness * material_cbuf.roughness_factor, metallic * material_cbuf.metallic_factor);

#endif
}