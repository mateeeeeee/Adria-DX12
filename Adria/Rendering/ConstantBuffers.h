#pragma once
#include "../Core/Definitions.h"
#include <DirectXMath.h>

#ifndef DECLSPEC_ALIGN
#define DECLSPEC_ALIGN(x)   __declspec(align(x))
#endif

namespace adria
{
	DECLSPEC_ALIGN(16) struct NewFrameCBuffer
	{
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX projection;
		DirectX::XMMATRIX view_projection;
		DirectX::XMMATRIX inverse_view;
		DirectX::XMMATRIX inverse_projection;
		DirectX::XMMATRIX inverse_view_projection;
		DirectX::XMMATRIX prev_view_projection;
		DirectX::XMVECTOR camera_position;
		DirectX::XMVECTOR camera_forward;
		float32  camera_near;
		float32  camera_far;
		float32  screen_resolution_x;
		float32  screen_resolution_y;

		float32  delta_time;
		float32  frame_count;

		float32  mouse_normalized_coords_x;
		float32  mouse_normalized_coords_y;

		int32    lights_idx;
	};

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
		float32 camera_near;
		float32 camera_far;
		float32 screen_resolution_x;
		float32 screen_resolution_y;
		float32 mouse_normalized_coords_x;
		float32 mouse_normalized_coords_y;
	};

	DECLSPEC_ALIGN(16) struct LightCBuffer
	{
		DirectX::XMVECTOR ss_position; //for postprocess effects
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		float32 range;
		int32	type;
		float32 outer_cosine;
		float32 inner_cosine;
		int32	casts_shadows;
		int32	use_cascades;
		int32	active;
		float32 volumetric_strength;
		int32	volumetric;
		int32	lens_flare;
		int32	god_rays;
		float32 godrays_decay;
		float32 godrays_weight;
		float32 godrays_density;
		float32 godrays_exposure;
		int32   sscs;
		float32 sscs_thickness;
		float32 sscs_max_ray_distance;
		float32 sscs_max_depth_distance;
	};

	DECLSPEC_ALIGN(16) struct ObjectCBuffer
	{
		DirectX::XMMATRIX model;
		DirectX::XMMATRIX inverse_transposed_model; 
	};

	DECLSPEC_ALIGN(16) struct MaterialCBuffer
	{
		DirectX::XMFLOAT3 ambient;
		float32 _padd1;
		DirectX::XMFLOAT3 diffuse;
		float32 alpha_cutoff;
		DirectX::XMFLOAT3 specular;

		float32 shininess;
		float32 albedo_factor;
		float32 metallic_factor;
		float32 roughness_factor;
		float32 emissive_factor;

		int32 albedo_idx = -1;
		int32 normal_idx = -1;
		int32 metallic_roughness_idx = -1;
		int32 emissive_idx = -1;
	};

	DECLSPEC_ALIGN(16) struct ShadowCBuffer
	{
		DirectX::XMMATRIX lightviewprojection;
		DirectX::XMMATRIX lightview;
		DirectX::XMMATRIX shadow_matrices[4];
		float32 split0;
		float32 split1;
		float32 split2;
		float32 split3;
		float32 softness;
		int32 shadow_map_size;
		int32 visualize;
	};

	DECLSPEC_ALIGN(16) struct PostprocessCBuffer
	{
		float32 velocity_buffer_scale;
		float32 tone_map_exposure;
		DirectX::XMVECTOR dof_params;
		int32	  tone_map_operator;
	};

	DECLSPEC_ALIGN(16) struct ComputeCBuffer
	{
		float32 bloom_scale;  //bloom
		float32 threshold;    //bloom

		float32 gauss_coeff1; //blur coefficients
		float32 gauss_coeff2; //blur coefficients
		float32 gauss_coeff3; //blur coefficients
		float32 gauss_coeff4; //blur coefficients
		float32 gauss_coeff5; //blur coefficients
		float32 gauss_coeff6; //blur coefficients
		float32 gauss_coeff7; //blur coefficients
		float32 gauss_coeff8; //blur coefficients
		float32 gauss_coeff9; //blur coefficients

		float32 bokeh_fallout;				//bokeh
		DirectX::XMVECTOR dof_params;		//bokeh
		float32 bokeh_radius_scale;			//bokeh
		float32 bokeh_color_scale;			//bokeh
		float32 bokeh_blur_threshold;		//bokeh
		float32 bokeh_lum_threshold;		//bokeh	

		int32 ocean_size;					//ocean
		int32 resolution;					//ocean
		float32 ocean_choppiness;			//ocean								
		float32 wind_direction_x;			//ocean
		float32 wind_direction_y;			//ocean
		float32 delta_time;					//ocean
		int32 visualize_tiled;				//tiled deferred
		int32 visualize_max_lights;			//tiled deferred
	};

	DECLSPEC_ALIGN(16) struct WeatherCBuffer
	{
		DirectX::XMVECTOR light_dir;
		DirectX::XMVECTOR light_color;
		DirectX::XMVECTOR sky_color;
		DirectX::XMVECTOR ambient_color;
		DirectX::XMVECTOR wind_dir;

		float32 wind_speed;
		float32 time;
		float32 crispiness;
		float32 curliness;

		float32 coverage;
		float32 absorption;
		float32 clouds_bottom_height;
		float32 clouds_top_height;

		float32 density_factor;
		float32 cloud_type;
		float32 _padd[2];

		//sky parameters
		DirectX::XMFLOAT3 A;
		float32 _paddA;
		DirectX::XMFLOAT3 B;
		float32 _paddB;
		DirectX::XMFLOAT3 C;
		float32 _paddC;
		DirectX::XMFLOAT3 D;
		float32 _paddD;
		DirectX::XMFLOAT3 E;
		float32 _paddE;
		DirectX::XMFLOAT3 F;
		float32 _paddF;
		DirectX::XMFLOAT3 G;
		float32 _paddG;
		DirectX::XMFLOAT3 H;
		float32 _paddH;
		DirectX::XMFLOAT3 I;
		float32 _paddI;
		DirectX::XMFLOAT3 Z;
		float32 _paddZ;
	};

	struct StructuredLight
	{
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		int32 active;
		float32 range;
		int32 type;
		float32 outer_cosine;
		float32 inner_cosine;
		int32 casts_shadows;
		int32 use_cascades;
		int32 padd;
	};

	struct ClusterAABB
	{
		DirectX::XMVECTOR min_point;
		DirectX::XMVECTOR max_point;
	};

	struct LightGrid
	{
		uint32 offset;
		uint32 light_count;
	};

	struct Bokeh
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 Size;
		DirectX::XMFLOAT3 Color;
	};
}