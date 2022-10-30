#include "../Util/LightUtil.hlsli"
#include "../Util/RootSignatures.hlsli"

#define GROUP_SIZE (16*16*1)
#define MAX_CLUSTER_LIGHTS 128
#define CLUSTER_CULLING_DISPATCH_SIZE_X 1
#define CLUSTER_CULLING_DISPATCH_SIZE_Y 1
#define CLUSTER_CULLING_DISPATCH_SIZE_Z 16

//references 
//https://github.com/pezcode/Cluster


struct ClusterAABB
{
    float4 min_point;
    float4 max_point;
};

struct LightGrid
{
    uint offset;
    uint light_count;
};


StructuredBuffer<ClusterAABB> clusters      : register(t0);
StructuredBuffer<StructuredLight> lights    : register(t1);

RWStructuredBuffer<uint> light_index_counter    : register(u0); //1
RWStructuredBuffer<uint> light_index_list       : register(u1); //MAX_CLUSTER_LIGHTS * 16^3
RWStructuredBuffer<LightGrid> light_grid        : register(u2); //16^3

groupshared StructuredLight shared_lights[GROUP_SIZE];


bool LightIntersectsCluster(StructuredLight light, ClusterAABB cluster)
{
    if (light.type == DIRECTIONAL_LIGHT)
        return true;

    float3 closest = max(cluster.min_point, min(light.position, cluster.max_point)).xyz;

    float3 dist = closest - light.position.xyz;
    return dot(dist, dist) <= (light.range * light.range);
}

[RootSignature(ClusterCulling_RS)]
[numthreads(16, 16, 1)]
void main(uint3 groupId : SV_GroupID,
          uint3 dispatchThreadId : SV_DispatchThreadID,
          uint3 groupThreadId : SV_GroupThreadID,
          uint groupIndex : SV_GroupIndex)
{
    
    if (all(dispatchThreadId == 0))
    {
        light_index_counter[0] = 0;
    }
        
    uint visible_light_count = 0;
    uint visible_light_indices[MAX_CLUSTER_LIGHTS];
    
    uint cluster_index = groupIndex + GROUP_SIZE * groupId.z;
    
    ClusterAABB cluster = clusters[cluster_index];
    

    uint light_offset = 0;
    uint light_count, dummy;
    lights.GetDimensions(light_count, dummy);
    
    
    while (light_offset < light_count)
    {
        uint batch_size = min(GROUP_SIZE, light_count - light_offset);
        
        if(groupIndex < batch_size)
        {
            uint light_index = light_offset + groupIndex;
            
            StructuredLight light = lights[light_index];
            
            shared_lights[groupIndex] = light;
        }
        
        GroupMemoryBarrierWithGroupSync();

        for (uint i = 0; i < batch_size; i++)
        {
            StructuredLight light = lights[i];
            
            if (!light.active || light.casts_shadows)
                continue;
            
            if (visible_light_count < MAX_CLUSTER_LIGHTS && LightIntersectsCluster(light, cluster))
            {
                visible_light_indices[visible_light_count] = light_offset + i;
                visible_light_count++;
            }
        }

        light_offset += batch_size;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    uint offset = 0;
    InterlockedAdd(light_index_counter[0], visible_light_count, offset);

    for (uint i = 0; i < visible_light_count; i++)
    {
        light_index_list[offset + i] = visible_light_indices[i];
    }
    
    light_grid[cluster_index].offset = offset; 
    light_grid[cluster_index].light_count = visible_light_count;
}










//https://www.3dgep.com/forward-plus/#Grid_Frustums_Compute_Shader
/*
function CullLights( L, C, G, I )
    Input: A set L of n lights.
    Input: A counter C of the current index into the global light index list.
    Input: A 2D grid G of index offset and count in the global light index list.
    Input: A list I of global light index list.
    Output: A 2D grid G with the current tiles offset and light count.
    Output: A list I with the current tiles overlapping light indices appended to it.
 
1.  let t be the index of the current tile  ; t is the 2D index of the tile.
2.  let i be a local light index list       ; i is a local light index list.
3.  let f <- Frustum(t)                     ; f is the frustum for the current tile.
 
4.  for l in L                      ; Iterate the lights in the light list.
5.      if Cull( l, f )             ; Cull the light against the tile frustum.
6.          AppendLight( l, i )     ; Append the light to the local light index list.
                     
7.  c <- AtomicInc( C, i.count )    ; Atomically increment the current index of the 
                                    ; global light index list by the number of lights
                                    ; overlapping the current tile and store the
                                    ; original index in c.
             
8.  G(t) <- ( c, i.count )          ; Store the offset and light count in the light grid.
         
9.  I(c) <- i                       ; Store the local light index list into the global 
                                    ; light index list.
*/