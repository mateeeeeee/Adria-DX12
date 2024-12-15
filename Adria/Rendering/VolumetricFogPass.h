#pragma once
#include "TextureHandle.h"
#include "Graphics/GfxDescriptor.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;
	class GfxComputePipelineState;
	class GfxGraphicsPipelineState;

	class VolumetricFogPass
	{
		struct FogVolumeGPU
		{
			Vector3 center;
			Vector3 extents;
			Vector3 color;
			Float   density_base;
			Float   density_change;
		};
		struct FogVolume
		{
			BoundingBox volume;
			Color		color;
			Float       density_base;
			Float       density_change;
		};
		static constexpr Uint32 BLUE_NOISE_TEXTURE_COUNT = 16;
	public:

		VolumetricFogPass(GfxDevice* gfx, entt::registry& reg, Uint32 w, Uint32 h);
		void AddPasses(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
			CreateLightInjectionHistoryTexture();
		}
		void OnSceneInitialized();
		void GUI();

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		Uint32 width, height;

		std::unique_ptr<GfxTexture> light_injection_target_history;
		GfxDescriptor light_injection_target_history_srv;

		std::vector<FogVolume> fog_volumes;
		std::unique_ptr<GfxBuffer> fog_volume_buffer;
		GfxDescriptor fog_volume_buffer_srv;
		Uint32 fog_volume_buffer_idx;

		std::array<TextureHandle, BLUE_NOISE_TEXTURE_COUNT> blue_noise_handles = { INVALID_TEXTURE_HANDLE };
		std::unique_ptr<GfxComputePipelineState>  light_injection_pso;
		std::unique_ptr<GfxComputePipelineState>  scattering_integration_pso;
		std::unique_ptr<GfxGraphicsPipelineState> combine_fog_pso;

	private:
		void CreatePSOs();
		void CreateLightInjectionHistoryTexture();
		void CreateFogVolumeBuffer();

		void AddLightInjectionPass(RenderGraph& rendergraph);
		void AddScatteringIntegrationPass(RenderGraph& rendergraph);
		void AddCombineFogPass(RenderGraph& rendergraph);
	};
}