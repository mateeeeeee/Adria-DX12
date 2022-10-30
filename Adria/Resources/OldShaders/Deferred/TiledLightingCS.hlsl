#include "../Globals/GlobalsCS.hlsli"
#include "../Util/RootSignatures.hlsli"

#define MAX_TILE_LIGHTS 256
#define TILED_LIGHTING_GROUP_SIZE 16


Texture2D normalMetallicTx                      : register(t0);
Texture2D diffuseRoughnessTx                    : register(t1);
Texture2D<float> depthTx                        : register(t2);
StructuredBuffer<StructuredLight> lights        : register(t3);

RWTexture2D<float4> outputTexture   : register(u0);
RWTexture2D<float4> debugTexture    : register(u1);

groupshared uint min_z;
groupshared uint max_z;
groupshared uint tile_light_indices[MAX_TILE_LIGHTS];
groupshared uint tile_num_lights;



[RootSignature(TiledLighting_RS)]
[numthreads(TILED_LIGHTING_GROUP_SIZE, TILED_LIGHTING_GROUP_SIZE, 1)]
void main(uint3 groupId : SV_GroupID,
          uint3 dispatchThreadId : SV_DispatchThreadID,
          uint3 groupThreadId : SV_GroupThreadID,
          uint  groupIndex : SV_GroupIndex)
{

    uint totalLights, _placeholder;
    lights.GetDimensions(totalLights, _placeholder);

    float min_z_pixel = frame_cbuf.camera_far;
    float max_z_pixel = frame_cbuf.camera_near;
    
    float2 tex = dispatchThreadId.xy / frame_cbuf.screen_resolution;

    float depth = depthTx.Load(int3(dispatchThreadId.xy, 0));
    float view_depth = ConvertZToLinearDepth(depth);

    bool validPixel = view_depth >= frame_cbuf.camera_near && view_depth < frame_cbuf.camera_far;
    
    [flatten]
    if (validPixel)
    {
        min_z_pixel = min(min_z_pixel, view_depth);
        max_z_pixel = max(max_z_pixel, view_depth);
    }

    if (groupIndex == 0)
    {
        tile_num_lights = 0;
        
        min_z = 0x7F7FFFFF; 
        max_z = 0;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (max_z_pixel >= min_z_pixel)
    {
        InterlockedMin(min_z, asuint(min_z_pixel));
        InterlockedMax(max_z, asuint(max_z_pixel));
    }

    GroupMemoryBarrierWithGroupSync();

    float min_tile_z = asfloat(min_z);
    float max_tile_z = asfloat(max_z);
    

    float2 tileScale = frame_cbuf.screen_resolution * rcp(float(2 * TILED_LIGHTING_GROUP_SIZE));
    float2 tileBias = tileScale - float2(groupId.xy);
    
    float4 c1 = float4(frame_cbuf.projection._11 * tileScale.x, 0.0f, tileBias.x, 0.0f);
    float4 c2 = float4(0.0f, -frame_cbuf.projection._22 * tileScale.y, tileBias.y, 0.0f);
    float4 c4 = float4(0.0f, 0.0f, 1.0f, 0.0f);

    // Derive frustum planes
    float4 frustumPlanes[6];
    // Sides
    frustumPlanes[0] = c4 - c1;
    frustumPlanes[1] = c4 + c1;
    frustumPlanes[2] = c4 - c2;
    frustumPlanes[3] = c4 + c2;
     // Near/far
    frustumPlanes[4] = float4(0.0f, 0.0f, 1.0f, -min_tile_z);
    frustumPlanes[5] = float4(0.0f, 0.0f, -1.0f, max_tile_z);
    
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        frustumPlanes[i] *= rcp(length(frustumPlanes[i].xyz));
    }
    
    
    for (uint lightIndex = groupIndex; lightIndex < totalLights; lightIndex += TILED_LIGHTING_GROUP_SIZE * TILED_LIGHTING_GROUP_SIZE)
    {
        StructuredLight light = lights[lightIndex];
                
        if (!light.active || light.casts_shadows)
            continue;
        
        bool inFrustum = true;
        
        if(light.type != DIRECTIONAL_LIGHT)
        {
            [unroll]
            for (uint i = 0; i < 6; ++i)
            {
                float d = dot(frustumPlanes[i], float4(light.position.xyz, 1.0f));
                inFrustum = inFrustum && (d >= -light.range);
            }
        }
        
    
        [branch]
        if (inFrustum)
        {
            uint listIndex;
            InterlockedAdd(tile_num_lights, 1, listIndex);
            tile_light_indices[listIndex] = lightIndex;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    float3 pos_vs = GetPositionVS(tex, depth);
    
    float4 NormalMetallic = normalMetallicTx.Load(int3(dispatchThreadId.xy, 0));
    
    float3 Normal = 2 * NormalMetallic.rgb - 1.0;
    float metallic = NormalMetallic.a;
    
    float4 AlbedoRoughness = diffuseRoughnessTx.Load(int3(dispatchThreadId.xy, 0));
    
    float3 V = normalize(0.0f.xxx - pos_vs);
    float roughness = AlbedoRoughness.a;
        
    float3 Lo = 0.0f;
    if (all(dispatchThreadId.xy < frame_cbuf.screen_resolution))
    {
        for (int i = 0; i < tile_num_lights; ++i)
        {
            
            Light light = CreateLightFromStructured(lights[tile_light_indices[i]]);
    
            switch (light.type)
            {
                case DIRECTIONAL_LIGHT:
                    Lo += DoDirectionalLightPBR(light, pos_vs, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
                    break;
                case POINT_LIGHT:
                    Lo += DoPointLightPBR(light, pos_vs, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
                    break;
                case SPOT_LIGHT:
                    Lo += DoSpotLightPBR(light, pos_vs, Normal, V, AlbedoRoughness.rgb, metallic, roughness);
                    break;
            }
        }
    }

    float4 shading_color = outputTexture[dispatchThreadId.xy] + float4(Lo, 1.0f);
    outputTexture[dispatchThreadId.xy] = shading_color;
    if (compute_cbuf.visualize_tiled)
    {
        const float3 heat_map[] =
        {
            float3(0, 0, 0),
		    float3(0, 0, 1),
		    float3(0, 1, 1),
		    float3(0, 1, 0),
		    float3(1, 1, 0),
		    float3(1, 0, 0),
        };
        const uint heap_map_max = 5;
        float l = saturate((float) tile_num_lights / compute_cbuf.visualize_max_lights) * heap_map_max;
        float3 a = heat_map[floor(l)];
        float3 b = heat_map[ceil(l)];
    
        float4 debug_color = float4(lerp(a, b, l - floor(l)), 0.5f);

        debugTexture[dispatchThreadId.xy] = debug_color;
    }

}