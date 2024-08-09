#pragma once
#include "HelperPasses.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class TiledDeferredLightingPass
	{
	public:
		TiledDeferredLightingPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			copy_to_texture_pass.OnResize(w, h);
			add_textures_pass.OnResize(w, h);
		}
	private:
		entt::registry& reg;
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> tiled_deferred_lighting_pso;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		bool visualize_tiled = false;
		int32 visualize_max_lights = 16;

	private:
		void CreatePSOs();
	};

}