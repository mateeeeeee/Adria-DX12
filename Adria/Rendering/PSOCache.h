#pragma once

namespace adria
{
	class GfxDevice;
	class GfxPipelineState;
	class GfxStateObject;

	enum class GfxPipelineStateID : uint8
	{
		SSR,
		GodRays,
		FilmEffects,
		ExponentialHeightFog,
		MotionBlur,
		MotionVectors,
		Picking,
		Unknown
	};

	namespace PSOCache
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();
		GfxPipelineState* Get(GfxPipelineStateID);
	};
}