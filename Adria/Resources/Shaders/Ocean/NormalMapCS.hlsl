#include "../Globals/GlobalsCS.hlsli"
#include "../Util/RootSignatures.hlsli"

#define WORK_GROUP_DIM 32
    
#define LAMBDA 1.2

Texture2D<float4> DisplacementMap : register(t0);
RWTexture2D<float4> NormalMap : register(u0);


[RootSignature(OceanNormal_RS)]
[numthreads(WORK_GROUP_DIM, WORK_GROUP_DIM, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{

    uint2 pixel_coord = DTid.xy;

    float texel = 1.f / compute_cbuf.resolution;
    float texel_size = compute_cbuf.ocean_size * texel;
    
   
    float3 _right  = DisplacementMap.Load(uint3(uint2(clamp(pixel_coord.x + 1, 0, compute_cbuf.resolution - 1), pixel_coord.y), 0)).xyz;
    float3 _left   = DisplacementMap.Load(uint3(uint2(clamp(pixel_coord.x - 1, 0, compute_cbuf.resolution - 1), pixel_coord.y), 0)).xyz;
    float3 _top    = DisplacementMap.Load(uint3(uint2(pixel_coord.x, clamp(pixel_coord.y - 1, 0, compute_cbuf.resolution - 1)), 0)).xyz;
    float3 _bottom = DisplacementMap.Load(uint3(uint2(pixel_coord.x, clamp(pixel_coord.y + 1, 0, compute_cbuf.resolution - 1)), 0)).xyz;
    

    float3 center   = DisplacementMap.Load(uint3(pixel_coord, 0)).xyz;
    float3 right    = float3( texel_size, 0.f, 0.f) + _right;
    float3 left     = float3(-texel_size, 0.f, 0.f) + _left;
    float3 top      = float3(0.f, 0.f, -texel_size) + _top;
    float3 bottom   = float3(0.f, 0.f,  texel_size) + _bottom;
    
    float3 top_right    = cross(right - center, top - center);
    float3 top_left     = cross(top - center, left - center);
    float3 bottom_left  = cross(left - center, bottom - center);
    float3 bottom_right = cross(bottom - center, right - center);
    
    
    // Jacobian
    float2 dDx = (_right.xz - _left.xz) * 0.5f; 
    float2 dDy = (_bottom.xz - _top.xz) * 0.5f;

    float J = (1.0 + dDx.x * LAMBDA) * (1.0 + dDy.y * LAMBDA) - dDx.y * dDy.x * LAMBDA * LAMBDA;
    
    float foam_factor = max(-J, 0.0); //negative J means foam

    NormalMap[pixel_coord] = float4(normalize(top_right + bottom_right + top_left + bottom_left), foam_factor);
    
    //float2 gradient = float2(_left.y - _right.y, _bottom.y - _top.y);
    //float3 normal = float3(gradient.x, 1, gradient.y);
    //NormalMap[pixel_coord] = float4(normal, foam_factor);
}

