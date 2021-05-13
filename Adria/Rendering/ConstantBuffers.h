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
		f32 camera_near;
		f32 camera_far;
		f32 screen_resolution_x;
		f32 screen_resolution_y;
		f32 fog_near;
		f32 fog_far;
	};

	DECLSPEC_ALIGN(16) struct LightCBuffer
	{
		DirectX::XMVECTOR ss_position; //for postprocess effects
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		f32 range;
		i32 type;
		f32 outer_cosine;
		f32 inner_cosine;
		i32 casts_shadows;
		i32 use_cascades;
		i32 active;
		f32 volumetric_strength;
		i32 volumetric;
		i32 lens_flare;
		i32 god_rays;
		f32 godrays_decay;
		f32 godrays_weight;
		f32 godrays_density;
		f32 godrays_exposure;
		i32 screenspace_shadows;
	};


	DECLSPEC_ALIGN(16) struct ObjectCBuffer
	{
		DirectX::XMMATRIX model;
		DirectX::XMMATRIX inverse_transposed_model; //jos nesto?
	};

	DECLSPEC_ALIGN(16) struct MaterialCBuffer
	{
		DirectX::XMFLOAT3 ambient;
		f32 _padd1;
		DirectX::XMFLOAT3 diffuse;
		f32 _padd2;
		DirectX::XMFLOAT3 specular;

		f32 shininess;
		f32 albedo_factor;
		f32 metallic_factor;
		f32 roughness_factor;
		f32 emissive_factor;
	};

	DECLSPEC_ALIGN(16) struct ShadowCBuffer
	{
		DirectX::XMMATRIX lightviewprojection;
		DirectX::XMMATRIX lightview;
		DirectX::XMMATRIX shadow_matrix1;
		DirectX::XMMATRIX shadow_matrix2;
		DirectX::XMMATRIX shadow_matrix3; //for cascades three 
		f32 split0;
		f32 split1;
		f32 split2;
		f32 softness;
		i32 shadow_map_size;
		i32 visualize;
	};

	DECLSPEC_ALIGN(16) struct PostprocessCBuffer
	{
		DirectX::XMFLOAT2 noise_scale;
		f32 ssao_radius;
		f32 ssao_power;
		DirectX::XMVECTOR samples[16];
		f32 ssr_ray_step;
		f32 ssr_ray_hit_threshold;
		f32 motion_blur_intensity;
		f32 _pad[1];
		DirectX::XMVECTOR dof_params;
		f32 tone_map_exposure;
		int tone_map_operator;
		
	};

	DECLSPEC_ALIGN(16) struct ComputeCBuffer
	{
		f32 bloom_scale;  //bloom
		f32 threshold;    //bloom

		f32 gauss_coeff1; //blur coefficients
		f32 gauss_coeff2; //blur coefficients
		f32 gauss_coeff3; //blur coefficients
		f32 gauss_coeff4; //blur coefficients
		f32 gauss_coeff5; //blur coefficients
		f32 gauss_coeff6; //blur coefficients
		f32 gauss_coeff7; //blur coefficients
		f32 gauss_coeff8; //blur coefficients
		f32 gauss_coeff9; //blur coefficients

		f32 bokeh_fallout;				//bokeh
		DirectX::XMVECTOR dof_params;	//bokeh
		f32 bokeh_radius_scale;			//bokeh
		f32 bokeh_color_scale;			//bokeh
		f32 bokeh_blur_threshold;		//bokeh
		f32 bokeh_lum_threshold;		//bokeh	

		i32 ocean_size;					//ocean
		i32 resolution;					//ocean
		f32 ocean_choppiness;			//ocean								
		f32 wind_direction_x;			//ocean
		f32 wind_direction_y;			//ocean
		f32 delta_time;					//ocean
		i32 visualize_tiled;			//tiled deferred
		i32 visualize_max_lights;		//tiled deferred
	};

	DECLSPEC_ALIGN(16) struct WeatherCBuffer
	{
		DirectX::XMVECTOR light_dir;
		DirectX::XMVECTOR light_color;
		DirectX::XMVECTOR ambient_color;
		DirectX::XMVECTOR wind_dir;
		f32 wind_speed;
		f32 time;
		f32 crispiness;
		f32 curliness;
		f32 coverage;
		f32 absorption;
		f32 clouds_bottom_height;
		f32 clouds_top_height;
		f32 density_factor;
		f32 cloud_type;
	};


	struct StructuredLight
	{
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		i32 active;
		f32 range;
		i32 type;
		f32 outer_cosine;
		f32 inner_cosine;
		i32 casts_shadows;
		i32 use_cascades;
		i32 padd;
	};
}