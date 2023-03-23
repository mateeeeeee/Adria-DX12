#pragma once
#include "Enums.h"


namespace adria
{

	struct PostprocessSettings
	{
		bool fog = false;
		AntiAliasing anti_aliasing = AntiAliasing_None;
		AmbientOcclusion ambient_occlusion = AmbientOcclusion::None;
		Reflections reflections = Reflections::SSR;
		bool dof = false;
		bool bokeh = false;
		bool bloom = false;
		bool clouds = false;
		bool motion_blur = false;
		bool automatic_exposure = false;
	};

	struct RendererSettings
	{
		RenderPathType		render_path = RenderPathType::RegularDeferred;
		PostprocessSettings postprocess{};
	};

}