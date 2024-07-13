#pragma once

namespace adria
{
	class GfxDevice;
	class GfxPipelineState;
	class GfxStateObject;

	enum class GfxPipelineStateID : uint8
	{
		Sun,
		Ambient,
		ToneMap,
		FXAA,
		TAA,
		SSAO,
		HBAO,
		SSR,
		GodRays,
		FilmEffects,
		DOF,
		ExponentialHeightFog,
		MotionBlur,
		Blur_Horizontal,
		Blur_Vertical,
		BokehGenerate,
		Bokeh,
		MotionVectors,
		Picking,
		RTAOFilter,
		Unknown
	};

	namespace PSOCache
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();
		GfxPipelineState* Get(GfxPipelineStateID);
	};
}