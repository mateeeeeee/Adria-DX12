#pragma once
#include "HelperPasses.h"
#include "RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class RayMarchedVolumetricFogPass
	{
	public:
		RayMarchedVolumetricFogPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~RayMarchedVolumetricFogPass();
		void AddPass(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
			copy_to_texture_pass.OnResize(w, h);
		}
		void GUI();

		void OnShadowTextureRendered(RGResourceName name)
		{
			shadow_textures.push_back(name);
		}
	private:
		GfxDevice* gfx;
		CopyToTexturePass copy_to_texture_pass;
		Uint32 width, height;
		std::vector<RGResourceName> shadow_textures;
		std::unique_ptr<GfxComputePipelineState> volumetric_lighting_pso;
		std::unique_ptr<GfxComputePipelineState> volumetric_lighting_pso_use_pcf;

	private:
		void CreatePSOs();
	};

}