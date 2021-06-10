#pragma once
#include "Enums.h"


namespace adria
{

	struct RendererSettings
	{
		//user settings
		f32 blur_sigma = 1.0f;
		f32 ambient_color[3] = { 15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f };
		f32 wind_direction[2] = { 10.0f, 10.0f };
		bool fog = false;
		FogType fog_type = FogType::eExponential;
		f32 fog_falloff = 0.02f;
		f32 fog_density = 0.02f;
		f32 fog_start = 100.0f;
		f32 fog_color[3] = { 0.5f,0.6f,0.7f };
		f32 shadow_softness = 1.0f;
		bool shadow_transparent = false;
		f32 split_lambda = 0.5f;
		AntiAliasing anti_aliasing = AntiAliasing_None;
		f32 tonemap_exposure = 1.0f;
		ToneMap tone_map_op = ToneMap::eHable;
		bool ssao = false;
		f32 ssao_power = 4.0f;
		f32 ssao_radius = 1.0f;
		bool ssr = false;
		f32 ssr_ray_step = 1.60f;
		f32 ssr_ray_hit_threshold = 2.00f;
		bool dof = false;
		f32 dof_near_blur = 000.0f;
		f32 dof_near = 200.0f;
		f32 dof_far = 400.0f;
		f32 dof_far_blur = 600.0f;
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
		f32 motion_blur_intensity = 32.0f;
		bool use_tiled_deferred = false;
		bool visualize_tiled = false;
		i32 visualize_max_lights = 16;
		bool use_clustered_deferred = false;
	};

}