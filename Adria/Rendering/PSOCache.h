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
		DeferredLighting,
		VolumetricLighting,
		TiledDeferredLighting,
		ClusteredDeferredLighting,
		ClusterBuilding,
		ClusterCulling,
		ToneMap,
		FXAA,
		TAA,
		SSAO,
		HBAO,
		SSR,
		GodRays,
		FilmEffects,
		LensFlare,
		LensFlare2,
		DOF,
		ExponentialHeightFog,
		MotionBlur,
		Blur_Horizontal,
		Blur_Vertical,
		BokehGenerate,
		Bokeh,
		GenerateMips,
		MotionVectors,
		Picking,
		BuildHistogram,
		HistogramReduction,
		Exposure,
		RTAOFilter,
		ReSTIRGI_InitialSampling,
		Unknown
	};

	namespace PSOCache
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();
		GfxPipelineState* Get(GfxPipelineStateID);
	};
}