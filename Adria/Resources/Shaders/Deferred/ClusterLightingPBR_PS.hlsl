
#include "../Globals/GlobalsPS.hlsli"
#include "../Util/DitherUtil.hlsli"
#include "../Util/RootSignatures.hlsli"


struct LightGrid
{
    uint offset;
    uint light_count;
};

SamplerState linear_wrap_sampler : register(s0);

Texture2D normalMetallicTx      : register(t0);
Texture2D diffuseRoughnessTx    : register(t1);
Texture2D<float> depthTx        : register(t2);


StructuredBuffer<StructuredLight> lights    : register(t3);
StructuredBuffer<uint> light_index_list     : register(t4);     //MAX_CLUSTER_LIGHTS * 16^3
StructuredBuffer<LightGrid> light_grid      : register(t5);      //16^3

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};

[RootSignature(ClusterLightingPBR_RS)]
float4 main(VertexOut pin) : SV_TARGET
{

    //unpack gbuffer
    float4 NormalMetallic = normalMetallicTx.Sample(linear_wrap_sampler, pin.Tex);
    float3 Normal = 2 * NormalMetallic.rgb - 1.0;
    float metallic = NormalMetallic.a;
    float depth = depthTx.Sample(linear_wrap_sampler, pin.Tex);
    float3 Position = GetPositionVS(pin.Tex, depth);
    float4 AlbedoRoughness = diffuseRoughnessTx.Sample(linear_wrap_sampler, pin.Tex);
    float3 V = normalize(0.0f.xxx - Position);
    float roughness = AlbedoRoughness.a;
    
    float linear_depth = ConvertZToLinearDepth(depth);

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    
    uint z_cluster = uint(max((log2(linear_depth) - log2(frame_cbuf.camera_near)) * 16.0f / log2(frame_cbuf.camera_far / frame_cbuf.camera_near), 0.0f));

    uint2 cluster_dim = ceil(frame_cbuf.screen_resolution / float2(16, 16));

    uint3 tiles = uint3(uint2(pin.PosH.xy / cluster_dim), z_cluster);

    uint tile_index = tiles.x +
                     16 * tiles.y +
                     (256) * tiles.z;
    
    uint light_count = light_grid[tile_index].light_count;
    uint light_offset = light_grid[tile_index].offset;

    for (uint i = 0; i < light_count; i++)
    {
        uint light_index = light_index_list[light_offset + i];
        StructuredLight light = lights[light_index];
        
        Light current_light = CreateLightFromStructured(light);

        switch (current_light.type)
        {
            case DIRECTIONAL_LIGHT:
                Lo += DoDirectionalLightPBR(current_light, Position, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
                break;
            case POINT_LIGHT:
                Lo += DoPointLightPBR(current_light, Position, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
                break;
            case SPOT_LIGHT:
                Lo += DoSpotLightPBR(current_light, Position, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
                break;
            default:
                return float4(1, 0, 0, 1);
        }
    }

    return float4(Lo, 1.0f);
}