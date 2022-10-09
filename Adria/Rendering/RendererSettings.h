#pragma once
#include "Enums.h"


namespace adria
{

	struct PostprocessSettings
	{
		bool fog = false;
		EAntiAliasing anti_aliasing = AntiAliasing_None;

		EAmbientOcclusion ambient_occlusion = EAmbientOcclusion::None;
		float32   ssao_power = 4.0f;
		float32   ssao_radius = 1.0f;
		float32   hbao_power = 1.5f;
		float32   hbao_radius = 2.0f;
		float32   rtao_radius = 2.0f;

		EReflections reflections = EReflections::SSR;
		bool dof = false;
		bool bokeh = false;
		bool bloom = false;
		bool clouds = false;
		bool motion_blur = false;
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