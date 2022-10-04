#include "LightUtil.hlsli"

struct FrameCBuffer
{
    float4 global_ambient;
    row_major matrix view;
    row_major matrix projection;
    row_major matrix viewprojection;
    row_major matrix inverse_view;
    row_major matrix inverse_projection;
    row_major matrix inverse_view_projection;
    row_major matrix prev_view_projection;
    float4 camera_position;
    float4 camera_forward;
    float camera_near;
    float camera_far;
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
    row_major matrix  shadow_matrix1;
    row_major matrix  shadow_matrix2;
    row_major matrix shadow_matrix3; //for cascades three 
    row_major matrix shadow_matrix4; //for cascades three 
    float4 splits;
    float softness;
    int shadow_map_size;
    int visualize;
};

#define EXPONENTIAL_FOG 0
#define EXPONENTIAL_HEIGHT_FOG 1



struct PostprocessCBuffer
{
    float2 noise_scale;
    float ssao_radius;
    float ssao_power;
    
    float4 samples[16];
    
    float ssr_ray_step;
    float ssr_ray_hit_threshold;
    float velocity_buffer_scale;
    float tone_map_exposure;
    
    float4 dof_params;
    float4 fog_color;
    
    float fog_falloff;
    float fog_density;
    float fog_start;
    int fog_type;
    
    float hbao_r2;
    float hbao_radius_to_screen;
    float hbao_power;
    int tone_map_op;

};


struct ComputeCBuffer
{
    float bloom_scale;  //bloom
    float threshold;    //bloom

    float gauss_coeff1; //blur coefficients
    float gauss_coeff2; //blur coefficients
    float gauss_coeff3; //blur coefficients
    float gauss_coeff4; //blur coefficients
    float gauss_coeff5; //blur coefficients
    float gauss_coeff6; //blur coefficients
    float gauss_coeff7; //blur coefficients
    float gauss_coeff8; //blur coefficients
    float gauss_coeff9; //blur coefficients

    float bokeh_fallout;        //bokeh
    float4 dof_params;          //bokeh
    float bokeh_radius_scale;   //bokeh
    float bokeh_color_scale;    //bokeh
    float bokeh_blur_threshold; //bokeh
    float bokeh_lum_threshold;  //bokeh	

    int ocean_size;             //ocean
    int resolution;             //ocean
    float ocean_choppiness;     //ocean								
    float wind_direction_x;     //ocean
    float wind_direction_y;     //ocean
    float delta_time;           //ocean

    int visualize_tiled;         //tiled deferred
    int visualize_max_lights;    //tiled deferred
};

struct WeatherCBuffer
{
    float4 light_dir;
    float4 light_color;
    float4 sky_color;
    float4 ambient_color;
    float4 wind_dir;
    float  wind_speed;
    float  time;
    float  crispiness;
    float  curliness;
    float  coverage;
    float  absorption;
    float  clouds_bottom_height;
    float  clouds_top_height;
    float  density_factor;
    float  cloud_type;
        
    float3 A;
    float3 B;
    float3 C;
    float3 D;
    float3 E;
    float3 F;
    float3 G;
    float3 H;
    float3 I;
    float3 Z;
};

struct RayTracingCBuffer
{
    float rtao_radius;
    int   frame_count;
    int   accumulated_frames;
    int   bounce_count;
};