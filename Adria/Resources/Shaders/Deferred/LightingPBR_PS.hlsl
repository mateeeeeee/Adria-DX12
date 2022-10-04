
#include "../Globals/GlobalsPS.hlsli"
#include "../Util/DitherUtil.hlsli"

Texture2D normalMetallicTx : register(t0);
Texture2D diffuseRoughnessTx : register(t1);
Texture2D<float> depthTx : register(t2);

SamplerState linear_wrap_sampler : register(s0);
SamplerState point_clamp_sampler : register(s1);


#include "../Util/ShadowUtil.hlsli"
#ifndef RAY_TRACED_SHADOWS
Texture2D       shadowDepthMap          : register(t4);
TextureCube     depthCubeMap            : register(t5);
Texture2DArray  cascadeDepthMap         : register(t6);
SamplerComparisonState shadow_sampler   : register(s2);
#else
Texture2D<float> rayTracedShadowMap  : register(t7);
#endif





struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


static const uint SSCS_MAX_STEPS = 16; 

float SSCS(float3 pos_vs)
{
    float3 ray_pos = pos_vs;
    float2 ray_uv = 0.0f;

    float4 ray_projected = mul(float4(ray_pos, 1.0f), frame_cbuf.projection);
    ray_projected.xy /= ray_projected.w;
    ray_uv = ray_projected.xy * float2(0.5f, -0.5f) + 0.5f;

    float depth = depthTx.Sample(point_clamp_sampler, ray_uv);
    float linear_depth = ConvertZToLinearDepth(depth);

    const float SSCS_STEP_LENGTH = light_cbuf.current_light.sscs_max_ray_distance / (float) SSCS_MAX_STEPS;

    if (linear_depth > light_cbuf.current_light.sscs_max_depth_distance)
        return 1.0f;

    float3 ray_direction = normalize(-light_cbuf.current_light.direction.xyz);
    float3 ray_step = ray_direction * SSCS_STEP_LENGTH;
    //ray_position += ray_step * dither(uv);

    float occlusion = 0.0f;
    [unroll(SSCS_MAX_STEPS)]
    for (uint i = 0; i < SSCS_MAX_STEPS; i++)
    {
        // Step the ray
        ray_pos += ray_step;

        ray_projected = mul(float4(ray_pos, 1.0), frame_cbuf.projection);
        ray_projected.xy /= ray_projected.w;
        ray_uv = ray_projected.xy * float2(0.5f, -0.5f) + 0.5f;

        [branch]
        if (IsSaturated(ray_uv))
        {
            depth = depthTx.Sample(point_clamp_sampler, ray_uv);
            //pute the difference between the ray's and the camera's depth
            linear_depth = ConvertZToLinearDepth(depth);
            float depth_delta = ray_projected.z - linear_depth;

            // Check if the camera can't "see" the ray (ray depth must be larger than the camera depth, so positive depth_delta)
            if (depth_delta > 0 && (depth_delta < light_cbuf.current_light.sscs_thickness))
            {
                // Mark as occluded
                occlusion = 1.0f;
                // screen edge fade:
                float2 fade = max(12 * abs(ray_uv - 0.5) - 5, 0);
                occlusion *= saturate(1 - dot(fade, fade));

                break;
            }
        }
    }

    return 1.0f - occlusion;
}


#include "../Util/RootSignatures.hlsli"
[RootSignature(LightingPBR_RS)]
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

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    switch (light_cbuf.current_light.type)
    {
        case DIRECTIONAL_LIGHT:
            Lo = DoDirectionalLightPBR(light_cbuf.current_light, Position, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
            break;
        case POINT_LIGHT:
            Lo = DoPointLightPBR(light_cbuf.current_light, Position, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
            break;
        case SPOT_LIGHT:
            Lo = DoSpotLightPBR(light_cbuf.current_light, Position, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
            break;
        default:
            return float4(1, 0, 0, 1);
    }

#ifndef RAY_TRACED_SHADOWS
    if (light_cbuf.current_light.casts_shadows)
    {

        float shadow_factor = 1.0f;
        if (light_cbuf.current_light.type == POINT_LIGHT)
        {
            const float zf = light_cbuf.current_light.range;
            const float zn = 0.5f;
            const float c1 = zf / (zf - zn);
            const float c0 = -zn * zf / (zf - zn);
            
            float3 light_to_pixelWS = mul(float4(Position - light_cbuf.current_light.position.xyz, 0.0f), frame_cbuf.inverse_view).xyz;

            const float3 m = abs(light_to_pixelWS).xyz;
            const float major = max(m.x, max(m.y, m.z));
            float fragment_depth = (c1 * major + c0) / major;
            shadow_factor = depthCubeMap.SampleCmpLevelZero(shadow_sampler, normalize(light_to_pixelWS.xyz), fragment_depth);
        }
        else if (light_cbuf.current_light.type == DIRECTIONAL_LIGHT && light_cbuf.current_light.use_cascades)
        {
            float viewDepth = Position.z;
            for (uint i = 0; i < 4; ++i)
            {
                matrix light_space_matrix = i == 0 ? shadow_cbuf.shadow_matrix1 : i == 1 ? shadow_cbuf.shadow_matrix2 : i == 2 ? shadow_cbuf.shadow_matrix3 : shadow_cbuf.shadow_matrix4;
                    
                float4 posShadowMap = mul(float4(Position, 1.0), light_space_matrix);
        
                float3 UVD = posShadowMap.xyz / posShadowMap.w;

                UVD.xy = 0.5 * UVD.xy + 0.5;
                UVD.y = 1.0 - UVD.y;

                if (viewDepth < shadow_cbuf.splits[i])
                {
                    shadow_factor = CSMCalcShadowFactor_PCF3x3(shadow_sampler, cascadeDepthMap, i, UVD, shadow_cbuf.shadow_map_size, shadow_cbuf.softness);
                      
                    break;
                }
            }    
        }
        else
        {
            float4 posShadowMap = mul(float4(Position, 1.0), shadow_cbuf.shadow_matrix1);
            float3 UVD = posShadowMap.xyz / posShadowMap.w;

            UVD.xy = 0.5 * UVD.xy + 0.5;
            UVD.y = 1.0 - UVD.y;
                
            shadow_factor = CalcShadowFactor_PCF3x3(shadow_sampler, shadowDepthMap, UVD, shadow_cbuf.shadow_map_size, shadow_cbuf.softness);
        }
        Lo = Lo * shadow_factor;
    }
    if (light_cbuf.current_light.sscs)
        Lo = Lo * SSCS(Position);
#else 
    float shadow_factor = rayTracedShadowMap.Sample(linear_wrap_sampler, pin.Tex).r;
    Lo = Lo * shadow_factor;
#endif
    return float4(Lo, 1.0f);
}
