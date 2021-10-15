#pragma once
#include "Enums.h"


namespace adria
{

	struct RendererSettings
	{
		//user settings
		f32 blur_sigma = 6.0f;
		f32 ambient_color[3] = { 15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f };
		f32 wind_direction[2] = { 10.0f, 10.0f };
		bool fog = false;
		EFogType fog_type = EFogType::Exponential;
		f32 fog_falloff = 0.005f;
		f32 fog_density = 0.002f;
		f32 fog_start = 100.0f;
		f32 fog_color[3] = { 0.5f,0.6f,0.7f };
		f32 shadow_softness = 1.0f;
		bool shadow_transparent = false;
		f32 split_lambda = 0.5f;
		EAntiAliasing anti_aliasing = AntiAliasing_None;
		f32 tonemap_exposure = 1.0f;
		EToneMap tone_map_op = EToneMap::Reinhard;
		EAmbientOcclusion ambient_occlusion = EAmbientOcclusion::None;
		f32   ssao_power = 4.0f;
		f32   ssao_radius = 1.0f;
		f32   hbao_power = 1.5f;
		f32   hbao_radius = 2.0f;
		bool ssr = false;
		f32 ssr_ray_step = 1.60f;
		f32 ssr_ray_hit_threshold = 2.00f;
		bool dof = false;
		bool bokeh = false;
		f32 dof_near_blur = 000.0f;
		f32 dof_near = 200.0f;
		f32 dof_far = 400.0f;
		f32 dof_far_blur = 600.0f;
		f32 bokeh_blur_threshold = 0.9f;
		f32 bokeh_lum_threshold = 1.0f;
		f32 bokeh_radius_scale = 25.0f;
		f32 bokeh_color_scale = 1.0f;
		f32 bokeh_fallout = 0.9f;
		EBokehType bokeh_type = EBokehType::Hex;
		bool bloom = false;
		f32 bloom_threshold = 0.25f;
		f32 bloom_scale = 2.0f;
		bool clouds = false;
		f32 crispiness = 43.0f;
		f32 curliness = 3.6f;
		f32 coverage = 0.505f;
		f32 wind_speed = 5.0f;
		f32 light_absorption = 0.003f;
		f32 clouds_bottom_height = 3000.0f;
		f32 clouds_top_height = 10000.0f;
		f32 density_factor = 0.015f;
		f32 cloud_type = 1.0f;
		bool motion_blur = false;
		f32 velocity_buffer_scale = 64.0f;
		bool use_tiled_deferred = false;
		bool visualize_tiled = false;
		i32 visualize_max_lights = 16;
		bool use_clustered_deferred = false;
		bool ibl = false;
		ESkyType sky_type = ESkyType::Skybox;
		f32 sky_color[3] = { 0.53f, 0.81f, 0.92f };
		f32 turbidity = 2.0f;
		f32 ground_albedo = 0.1f;
	};

}