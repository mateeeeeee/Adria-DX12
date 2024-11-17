#pragma once
#include "Graphics/GfxDescriptor.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class GfxBuffer;
	class GfxTexture;
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class ReSTIRGI
	{
		enum class ResamplingMode : Uint8
		{
			None = 0,
			Temporal = 1,
			Spatial = 2,
			TemporalAndSpatial = 3
		};

	public:
		ReSTIRGI(GfxDevice* gfx, Uint32 width, Uint32 height);

		void AddPasses(RenderGraph& rg);
		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
		}
		Bool IsSupported() const { return supported; }

	private:
		GfxDevice* gfx;
		Uint32 width, height;

		Bool supported = false;
		Bool enable = false;
		ResamplingMode resampling_mode = ResamplingMode::TemporalAndSpatial;

		struct TemporalReservoirBuffers
		{
			std::unique_ptr<GfxTexture> sample_radiance;
			std::unique_ptr<GfxTexture> ray_direction;
			std::unique_ptr<GfxTexture> reservoir;
		};
		TemporalReservoirBuffers temporal_reservoir_buffers[2];

		std::unique_ptr<GfxComputePipelineState> initial_sampling_pso;
		std::unique_ptr<GfxComputePipelineState> temporal_resampling_pso;
		std::unique_ptr<GfxComputePipelineState> spatial_resampling_pso;

	private:
		void AddInitialSamplingPass(RenderGraph& rg);
		void AddTemporalResamplingPass(RenderGraph& rg);
		void AddSpatialResamplingPass(RenderGraph& rg);

		void CreateBuffers();
	};
}