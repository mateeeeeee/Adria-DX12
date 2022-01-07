#pragma once
#include "../Core/Definitions.h"
#include <DirectXMath.h>

#ifndef DECLSPEC_ALIGN
#define DECLSPEC_ALIGN(x)   __declspec(align(x))
#endif

namespace adria
{
	DECLSPEC_ALIGN(16) struct FrameCBuffer
	{
		DirectX::XMVECTOR global_ambient;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX projection;
		DirectX::XMMATRIX view_projection;
		DirectX::XMMATRIX inverse_view;
		DirectX::XMMATRIX inverse_projection;
		DirectX::XMMATRIX inverse_view_projection;
		DirectX::XMMATRIX prev_view_projection;
		DirectX::XMVECTOR camera_position;
		DirectX::XMVECTOR camera_forward;
		F32 camera_near;
		F32 camera_far;
		F32 screen_resolution_x;
		F32 screen_resolution_y;
	};

	DECLSPEC_ALIGN(16) struct LightCBuffer
	{
		DirectX::XMVECTOR ss_position; //for postprocess effects
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		F32 range;
		I32 type;
		F32 outer_cosine;
		F32 inner_cosine;
		I32 casts_shadows;
		I32 use_cascades;
		I32 active;
		F32 volumetric_strength;
		I32 volumetric;
		I32 lens_flare;
		I32 god_rays;
		F32 godrays_decay;
		F32 godrays_weight;
		F32 godrays_density;
		F32 godrays_exposure;
		I32 screenspace_shadows;
	};

	DECLSPEC_ALIGN(16) struct ObjectCBuffer
	{
		DirectX::XMMATRIX model;
		DirectX::XMMATRIX inverse_transposed_model; //jos nesto?
	};

	DECLSPEC_ALIGN(16) struct MaterialCBuffer
	{
		DirectX::XMFLOAT3 ambient;
		F32 _padd1;
		DirectX::XMFLOAT3 diffuse;
		F32 _padd2;
		DirectX::XMFLOAT3 specular;

		F32 shininess;
		F32 albedo_factor;
		F32 metallic_factor;
		F32 roughness_factor;
		F32 emissive_factor;
	};

	DECLSPEC_ALIGN(16) struct ShadowCBuffer
	{
		DirectX::XMMATRIX lightviewprojection;
		DirectX::XMMATRIX lightview;
		DirectX::XMMATRIX shadow_matrix1;
		DirectX::XMMATRIX shadow_matrix2;
		DirectX::XMMATRIX shadow_matrix3; //for cascades three 
		F32 split0;
		F32 split1;
		F32 split2;
		F32 softness;
		I32 shadow_map_size;
		I32 visualize;
	};

	DECLSPEC_ALIGN(16) struct PostprocessCBuffer
	{
		DirectX::XMFLOAT2 noise_scale;
		F32 ssao_radius;
		F32 ssao_power;

		DirectX::XMVECTOR samples[16];

		F32 ssr_ray_step;
		F32 ssr_ray_hit_threshold;
		F32 velocity_buffer_scale;
		F32 tone_map_exposure;

		DirectX::XMVECTOR dof_params;
		DirectX::XMVECTOR fog_color;
		
		F32 fog_falloff;
		F32 fog_density;
		F32 fog_start;
		I32 fog_type;

		F32   hbao_r2;
		F32   hbao_radius_to_screen;
		F32   hbao_power;
		I32	  tone_map_operator;
	};

	DECLSPEC_ALIGN(16) struct ComputeCBuffer
	{
		F32 bloom_scale;  //bloom
		F32 threshold;    //bloom

		F32 gauss_coeff1; //blur coefficients
		F32 gauss_coeff2; //blur coefficients
		F32 gauss_coeff3; //blur coefficients
		F32 gauss_coeff4; //blur coefficients
		F32 gauss_coeff5; //blur coefficients
		F32 gauss_coeff6; //blur coefficients
		F32 gauss_coeff7; //blur coefficients
		F32 gauss_coeff8; //blur coefficients
		F32 gauss_coeff9; //blur coefficients

		F32 bokeh_fallout;				//bokeh
		DirectX::XMVECTOR dof_params;	//bokeh
		F32 bokeh_radius_scale;			//bokeh
		F32 bokeh_color_scale;			//bokeh
		F32 bokeh_blur_threshold;		//bokeh
		F32 bokeh_lum_threshold;		//bokeh	

		I32 ocean_size;					//ocean
		I32 resolution;					//ocean
		F32 ocean_choppiness;			//ocean								
		F32 wind_direction_x;			//ocean
		F32 wind_direction_y;			//ocean
		F32 delta_time;					//ocean
		I32 visualize_tiled;			//tiled deferred
		I32 visualize_max_lights;		//tiled deferred
	};

	DECLSPEC_ALIGN(16) struct WeatherCBuffer
	{
		DirectX::XMVECTOR light_dir;
		DirectX::XMVECTOR light_color;
		DirectX::XMVECTOR sky_color;
		DirectX::XMVECTOR ambient_color;
		DirectX::XMVECTOR wind_dir;

		F32 wind_speed;
		F32 time;
		F32 crispiness;
		F32 curliness;

		F32 coverage;
		F32 absorption;
		F32 clouds_bottom_height;
		F32 clouds_top_height;

		F32 density_factor;
		F32 cloud_type;
		F32 _padd[2];

		//sky parameters
		DirectX::XMFLOAT3 A;
		F32 _paddA;
		DirectX::XMFLOAT3 B;
		F32 _paddB;
		DirectX::XMFLOAT3 C;
		F32 _paddC;
		DirectX::XMFLOAT3 D;
		F32 _paddD;
		DirectX::XMFLOAT3 E;
		F32 _paddE;
		DirectX::XMFLOAT3 F;
		F32 _paddF;
		DirectX::XMFLOAT3 G;
		F32 _paddG;
		DirectX::XMFLOAT3 H;
		F32 _paddH;
		DirectX::XMFLOAT3 I;
		F32 _paddI;
		DirectX::XMFLOAT3 Z;
		F32 _paddZ;
	};

	DECLSPEC_ALIGN(16) struct RayTracingCBuffer
	{
		F32 rtao_radius;
		I32 frame_count;
	};


	struct StructuredLight
	{
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		I32 active;
		F32 range;
		I32 type;
		F32 outer_cosine;
		F32 inner_cosine;
		I32 casts_shadows;
		I32 use_cascades;
		I32 padd;
	};

	struct ClusterAABB
	{
		DirectX::XMVECTOR min_point;
		DirectX::XMVECTOR max_point;
	};

	struct LightGrid
	{
		U32 offset;
		U32 light_count;
	};

	struct Bokeh
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 Size;
		DirectX::XMFLOAT3 Color;
	};
}