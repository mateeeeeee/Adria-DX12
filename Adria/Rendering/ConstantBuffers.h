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
		DirectX::XMMATRIX reprojection;
		DirectX::XMVECTOR camera_position;
		DirectX::XMVECTOR camera_forward;
		float  camera_near;
		float  camera_far;
		float  screen_resolution_x;
		float  screen_resolution_y;

		float  delta_time;
		float  total_time;
		uint32   frame_count;

		float  mouse_normalized_coords_x;
		float  mouse_normalized_coords_y;

		int32    lights_idx;
	};

	DECLSPEC_ALIGN(16) struct FrameCBuffer
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
		float camera_near;
		float camera_far;
		float screen_resolution_x;
		float screen_resolution_y;
		float mouse_normalized_coords_x;
		float mouse_normalized_coords_y;
	};

	DECLSPEC_ALIGN(16) struct LightCBuffer
	{
		DirectX::XMVECTOR ss_position; //for postprocess effects
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		float range;
		int32	type;
		float outer_cosine;
		float inner_cosine;
		int32	casts_shadows;
		int32	use_cascades;
		int32	active;
		float volumetric_strength;
		int32	volumetric;
		int32	lens_flare;
		int32	god_rays;
		float godrays_decay;
		float godrays_weight;
		float godrays_density;
		float godrays_exposure;
		int32   sscs;
		float sscs_thickness;
		float sscs_max_ray_distance;
		float sscs_max_depth_distance;
	};

	DECLSPEC_ALIGN(16) struct ObjectCBuffer
	{
		DirectX::XMMATRIX model;
		DirectX::XMMATRIX inverse_transposed_model; 
	};

	DECLSPEC_ALIGN(16) struct MaterialCBuffer
	{
		DirectX::XMFLOAT3 ambient;
		float _padd1;
		DirectX::XMFLOAT3 diffuse;
		float alpha_cutoff;
		DirectX::XMFLOAT3 specular;

		float shininess;
		float albedo_factor;
		float metallic_factor;
		float roughness_factor;
		float emissive_factor;

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
		float split0;
		float split1;
		float split2;
		float split3;
		float softness;
		int32 shadow_map_size;
		int32 visualize;
	};

	DECLSPEC_ALIGN(16) struct ComputeCBuffer
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

		float bokeh_fallout;				//bokeh
		DirectX::XMVECTOR dof_params;		//bokeh
		float bokeh_radius_scale;			//bokeh
		float bokeh_color_scale;			//bokeh
		float bokeh_blur_threshold;		//bokeh
		float bokeh_lum_threshold;		//bokeh	

		int32 ocean_size;					//ocean
		int32 resolution;					//ocean
		float ocean_choppiness;			//ocean								
		float wind_direction_x;			//ocean
		float wind_direction_y;			//ocean
		float delta_time;					//ocean
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

		float wind_speed;
		float time;
		float crispiness;
		float curliness;

		float coverage;
		float absorption;
		float clouds_bottom_height;
		float clouds_top_height;

		float density_factor;
		float cloud_type;
		float _padd[2];

		//sky parameters
		DirectX::XMFLOAT3 A;
		float _paddA;
		DirectX::XMFLOAT3 B;
		float _paddB;
		DirectX::XMFLOAT3 C;
		float _paddC;
		DirectX::XMFLOAT3 D;
		float _paddD;
		DirectX::XMFLOAT3 E;
		float _paddE;
		DirectX::XMFLOAT3 F;
		float _paddF;
		DirectX::XMFLOAT3 G;
		float _paddG;
		DirectX::XMFLOAT3 H;
		float _paddH;
		DirectX::XMFLOAT3 I;
		float _paddI;
		DirectX::XMFLOAT3 Z;
		float _paddZ;
	};

	struct StructuredLight
	{
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR color;
		int32 active;
		float range;
		int32 type;
		float outer_cosine;
		float inner_cosine;
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