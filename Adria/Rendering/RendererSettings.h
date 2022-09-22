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

		EReflections reflections = EReflections::None;
		float32 ssr_ray_step = 1.60f;
		float32 ssr_ray_hit_threshold = 2.00f;

		bool dof = false;
		float32 dof_near_blur = 000.0f;
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
	};

	struct RendererSettings
	{
		bool gui_visible = false;
		//user settings
		float32 blur_sigma = 6.0f;
		float32 ambient_color[3] = { 60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f };
		float32 wind_direction[2] = { 10.0f, 10.0f };
		float32 shadow_softness = 1.0f;
		bool shadow_transparent = false;
		float32 split_lambda = 0.5f;
		bool use_tiled_deferred = false;
		bool visualize_tiled = false;
		int32 visualize_max_lights = 16;
		bool use_clustered_deferred = false;
		bool use_path_tracing = false;
		int32 bounces = 3;
		bool ibl = false;
		ESkyType sky_type = ESkyType::Skybox;
		float32 sky_color[3] = { 0.53f, 0.81f, 0.92f };
		float32 turbidity = 2.0f;
		float32 ground_albedo = 0.1f;
		bool ocean_wireframe = false;
		bool ocean_tesselation = false;
		float32 ocean_color[3] = { 0.0123f, 0.3613f, 0.6867f };
		float32 ocean_choppiness = 1.2f;
		bool ocean_color_changed = false;
		bool recreate_initial_spectrum = true;
		PostprocessSettings postprocessor{};
	};

}