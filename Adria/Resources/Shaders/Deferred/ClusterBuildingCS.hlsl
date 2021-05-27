#include "../Globals/GlobalsCS.hlsli"
#include "../Util/RootSignatures.hlsli"

//reference
//https://github.com/Angelo1211/HybridRenderingEngine/


#define CLUSTER_BUILDING_DISPATCH_SIZE_X 16
#define CLUSTER_BUILDING_DISPATCH_SIZE_Y 16
#define CLUSTER_BUILDING_DISPATCH_SIZE_Z 16


struct ClusterAABB
{
    float4 min_point;
    float4 max_point;
};

RWStructuredBuffer<ClusterAABB> clusters : register(u0);

float3 IntersectionZPlane(float3 B, float z_dist)
{
    //Because this is a Z based normal this is fixed
    float3 normal = float3(0.0, 0.0, -1.0);
    float3 d = B;
    //Computing the intersection length for the line and the plane
    float t = z_dist / d.z; //dot(normal, d);
    
    //Computing the actual xyz position of the point along the line
    float3 result = t * d;

    return result;
}

[RootSignature(ClusterBuilding_RS)]
[numthreads(1, 1, 1)]
void main(uint3 groupId             : SV_GroupID,
          uint3 dispatchThreadId    : SV_DispatchThreadID,
          uint3 groupThreadId       : SV_GroupThreadID,
          uint  groupIndex          : SV_GroupIndex)
{
    uint cluster_size, dummy;
    clusters.GetDimensions(cluster_size, dummy);
    
    uint tile_index = groupId.x +
                      groupId.y * CLUSTER_BUILDING_DISPATCH_SIZE_X +
                      groupId.z * (CLUSTER_BUILDING_DISPATCH_SIZE_X * CLUSTER_BUILDING_DISPATCH_SIZE_Y);
    
    float2 tile_size = rcp(float2(CLUSTER_BUILDING_DISPATCH_SIZE_X, CLUSTER_BUILDING_DISPATCH_SIZE_Y)); //screen_resolution;

    float3 max_point_vs = GetPositionVS((groupId.xy + 1) * tile_size, 1.0f);
    float3 min_point_vs = GetPositionVS(groupId.xy * tile_size, 1.0f);

    float cluster_near = frame_cbuf.camera_near * pow(frame_cbuf.camera_far / frame_cbuf.camera_near, groupId.z / float(CLUSTER_BUILDING_DISPATCH_SIZE_Z));
    float cluster_far = frame_cbuf.camera_near * pow(frame_cbuf.camera_far / frame_cbuf.camera_near, (groupId.z + 1) / float(CLUSTER_BUILDING_DISPATCH_SIZE_Z));

    float3 minPointNear = IntersectionZPlane(min_point_vs, cluster_near);
    float3 minPointFar  = IntersectionZPlane(min_point_vs, cluster_far);
    float3 maxPointNear = IntersectionZPlane(max_point_vs, cluster_near);
    float3 maxPointFar  = IntersectionZPlane(max_point_vs, cluster_far);

    float3 minPointAABB = min(min(minPointNear, minPointFar), min(maxPointNear, maxPointFar));
    float3 maxPointAABB = max(max(minPointNear, minPointFar), max(maxPointNear, maxPointFar));

    clusters[tile_index].min_point = float4(minPointAABB, 0.0);
    clusters[tile_index].max_point = float4(maxPointAABB, 0.0);
    

}