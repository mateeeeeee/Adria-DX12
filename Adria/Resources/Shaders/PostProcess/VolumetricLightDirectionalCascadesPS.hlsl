
#include "../Globals/GlobalsPS.hlsli"
#include "../Util/DitherUtil.hlsli"
#include "../Util/ShadowUtil.hlsli"
#include "../Util/RootSignatures.hlsli"


Texture2D<float> depthTx         : register(t0);
Texture2DArray cascadeShadowMaps : register(t1);

SamplerComparisonState shadow_sampler : register(s1);
SamplerState linear_clamp_sampler : register(s2);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


[RootSignature(Volumetric_RS)]
float4 main(VertexOut input) : SV_TARGET
{

    
    float depth = max(input.PosH.z, depthTx.SampleLevel(linear_clamp_sampler, input.Tex, 2));
    float3 P = GetPositionVS(input.Tex, depth);
    float3 V = float3(0.0f, 0.0f, 0.0f) - P;
    float cameraDistance = length(V);
    V /= cameraDistance;

    float marchedDistance = 0;
    float3 accumulation = 0;

    const float3 L = light_cbuf.current_light.direction.xyz;

    float3 rayEnd = float3(0.0f, 0.0f, 0.0f);

    const uint sampleCount = 16;
    const float stepSize = length(P - rayEnd) / sampleCount;


    P = P + V * stepSize * dither(input.PosH.xy);
    float viewDepth = P.z;
    
	[loop]
    for (uint j = 0; j < sampleCount; ++j)
    {
        bool valid = false;
        for (uint cascade = 0; cascade < 3; ++cascade)
        {
            matrix light_space_matrix = cascade == 0 ? shadow_cbuf.shadow_matrix1 : cascade == 1 ? shadow_cbuf.shadow_matrix2 : shadow_cbuf.shadow_matrix3;
                    
            float4 posShadowMap = mul(float4(P, 1.0), light_space_matrix);
        
            float3 UVD = posShadowMap.xyz / posShadowMap.w;

            UVD.xy = 0.5 * UVD.xy + 0.5;
            UVD.y = 1.0 - UVD.y;

            [branch]
            if (viewDepth < shadow_cbuf.splits[cascade])
            {
                if (IsSaturated(UVD.xy))
                {
                    float attenuation = CSMCalcShadowFactor_PCF3x3(shadow_sampler, cascadeShadowMaps, cascade, UVD, shadow_cbuf.shadow_map_size, shadow_cbuf.softness);

                    attenuation *= GetFogAmount(cameraDistance - marchedDistance);

                    accumulation += attenuation;
                }

                marchedDistance += stepSize;
                P = P + V * stepSize;

                valid = true;
                break;
            }
        }

        if (!valid)
        {
            break;
        }

    }

    accumulation /= sampleCount;

    return max(0, float4(accumulation * light_cbuf.current_light.color.rgb * light_cbuf.current_light.volumetric_strength, 1));
}
