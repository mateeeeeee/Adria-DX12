#pragma once

namespace adria
{

	enum class AmbientOcclusion : uint8
	{
		None,
		SSAO,
		HBAO,
		RTAO
	};
	enum class Reflections : uint8
	{
		None,
		SSR,
		RTR
	};
	enum AntiAliasing : uint8
	{
		AntiAliasing_None = 0x0,
		AntiAliasing_FXAA = 0x1,
		AntiAliasing_TAA = 0x2
	};

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


	enum class RenderPathType
	{
		RegularDeferred,
		TiledDeferred,
		ClusteredDeferred,
		PathTracing
	};
	struct RendererSettings
	{
		RenderPathType		render_path = RenderPathType::RegularDeferred;
		PostprocessSettings postprocess{};
	};

}