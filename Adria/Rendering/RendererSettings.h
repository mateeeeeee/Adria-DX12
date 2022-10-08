#pragma once
#include "Enums.h"


namespace adria
{

	struct PostprocessSettings
	{
		bool fog = false;

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
		bool dof = false;
		bool bokeh = false;
		float32 bokeh_blur_threshold = 0.9f;
		float32 bokeh_lum_threshold = 1.0f;
		float32 bokeh_radius_scale = 25.0f;
		float32 bokeh_color_scale = 1.0f;
		float32 bokeh_fallout = 0.9f;
		EBokehType bokeh_type = EBokehType::Hex;

		bool bloom = false;

		bool clouds = false;
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