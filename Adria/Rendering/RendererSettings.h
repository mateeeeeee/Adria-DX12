#pragma once
#include "Enums.h"


namespace adria
{

	struct PostprocessSettings
	{
		bool fog = false;
		AntiAliasing anti_aliasing = AntiAliasing_TAA;
		AmbientOcclusion ambient_occlusion = AmbientOcclusion::SSAO;
		Reflections reflections = Reflections::SSR;
		bool dof = false;
		bool bokeh = false;
		bool bloom = false;
		bool clouds = true;
		bool motion_blur = false;
		bool automatic_exposure = true;
	};

	struct RendererSettings
	{
		RenderPathType		render_path = RenderPathType::RegularDeferred;
		PostprocessSettings postprocess{};
	};

}