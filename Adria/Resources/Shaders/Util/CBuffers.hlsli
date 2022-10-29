#include "LightUtil.hlsli"

struct FrameCBuffer
{
    row_major matrix view;
    row_major matrix projection;
    row_major matrix viewprojection;
    row_major matrix inverse_view;
    row_major matrix inverse_projection;
    row_major matrix inverse_view_projection;
    row_major matrix prev_view_projection;
    float4 camera_position;
    float4 camera_forward;
	float  camera_near;
	float  camera_far;
    float2 screen_resolution;
    float2 mouse_normalized_coords;
};

struct LightCBuffer
{
    Light current_light;
};

struct ObjectCBuffer
{
    row_major matrix model;
    row_major matrix inverse_transposed_model;
};

struct MaterialCBuffer
{
    float3 ambient;
    float3 diffuse;
    float  alpha_cutoff;
    float3 specular;
    float shininess;
    float albedo_factor;
    float metallic_factor;
    float roughness_factor;
    float emissive_factor;
    
    int albedo_idx;
    int normal_idx;
    int metallic_roughness_idx;
    int emissive_idx;
};


struct ShadowCBuffer
{
    row_major matrix  lightviewprojection;
    row_major matrix  lightview;
    row_major matrix  shadow_matrices[4];
    float4 splits;
    float softness;
    int shadow_map_size;
    int visualize;
};

struct ComputeCBuffer
{
    int visualize_tiled;         //tiled deferred
    int visualize_max_lights;    //tiled deferred
};
