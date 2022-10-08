#pragma once
#include "Enums.h"


namespace adria
{

	struct PostprocessSettings
	{
		bool fog = false;
		EFogType fog_type = EFogType::Exponential;
		float32 fog_falloff = 0.005f;
		float32 fog_density = 0.002f;
		float32 fog_start = 100.0f;
		float32 fog_color[3] = { 0.5f,0.6f,0.7f };

		EAntiAliasing anti_aliasing = AntiAliasing_None;

		EToneMap tone_map_op = EToneMap::Reinhard;
		float32 tonemap_exposure = 1.0f;

		EAmbientOcclusion ambient_occlusion = EAmbientOcclusion::None;
		float32   ssao_power = 4.0f;
		float32   ssao_radius = 1.0f;
		float32   hbao_power = 1.5f;
		float32   hbao_radius = 2.0f;
		float32   rtao_radius = 2.0f;

		EReflections reflections = EReflections::SSR;
		float32 ssr_ray_step = 1.60f;
		float32 ssr_ray_hit_threshold = 2.00f;

		bool dof = false;
		float32 dof_near_blur = 0.0f;
		float32 dof_near = 200.0f;
		float32 dof_far = 400.0f;
		float32 dof_far_blur = 600.0f;

		bool bokeh = false;
		float32 bokeh_blur_threshold = 0.9f;
		float32 bokeh_lum_threshold = 1.0f;
		float32 bokeh_radius_scale = 25.0f;
		float32 bokeh_color_scale = 1.0f;
		float32 bokeh_fallout = 0.9f;
		EBokehType bokeh_type = EBokehType::Hex;

		bool bloom = false;
		float32 bloom_threshold = 0.25f;
		float32 bloom_scale = 2.0f;

		bool clouds = false;
		float32 crispiness = 43.0f;
		float32 curliness = 3.6f;
		float32 coverage = 0.505f;
		float32 wind_speed = 5.0f;
		float32 light_absorption = 0.003f;
		float32 clouds_bottom_height = 3000.0f;
		float32 clouds_top_height = 10000.0f;
		float32 density_factor = 0.015f;
		float32 cloud_type = 1.0f;

		bool motion_blur = false;
		float32 velocity_buffer_scale = 64.0f;
		bool automatic_exposure = false;
	};

	struct RendererSettings
	{
		float32 ambient_color[3] = { 15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f };
		bool ibl = false;
		bool gui_visible = false;
		bool use_tiled_deferred = false;
		bool use_clustered_deferred = false;
		PostprocessSettings postprocessor{};
	};

}