#pragma once
#include "TextureHandle.h"
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class RenderGraph;
	class TextureManager;
	class GfxDevice;
	class GfxTexture;
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;

	class OceanRenderer
	{
		static constexpr Uint32 FFT_RESOLUTION = 512;

	public:
		OceanRenderer(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);
		~OceanRenderer();

		void AddPasses(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);
		void OnSceneInitialized();

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		TextureHandle foam_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle perlin_handle = INVALID_TEXTURE_HANDLE;
		Uint32 width, height;
		std::unique_ptr<GfxTexture> initial_spectrum;
		std::unique_ptr<GfxTexture> ping_pong_phase_textures[2];
		Bool pong_phase = false;
		std::unique_ptr<GfxTexture> ping_pong_spectrum_textures[2];
		Bool pong_spectrum = false;

		std::unique_ptr<GfxGraphicsPipelineStatePermutations> ocean_psos;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> ocean_lod_psos;
		std::unique_ptr<GfxComputePipelineState> fft_horizontal_pso;
		std::unique_ptr<GfxComputePipelineState> fft_vertical_pso;
		std::unique_ptr<GfxComputePipelineState> initial_spectrum_pso;
		std::unique_ptr<GfxComputePipelineState> spectrum_pso;
		std::unique_ptr<GfxComputePipelineState> phase_pso;
		std::unique_ptr<GfxComputePipelineState> ocean_normals_pso;

		//settings
		Bool ocean_wireframe = false;
		Bool ocean_tesselation = false;
		Float ocean_color[3] = { 0.0123f, 0.3613f, 0.6867f };
		Float ocean_choppiness = 1.2f;
		Bool ocean_color_changed = false;
		Bool recreate_initial_spectrum = true;
		Float wind_direction[2] = { 10.0f, 10.0f };

	private:
		void CreatePSOs();
	};
}