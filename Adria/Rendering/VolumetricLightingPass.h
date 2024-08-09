#pragma once
#include "HelperPasses.h"
#include "RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class VolumetricLightingPass
	{
		enum VolumetricLightingResolution
		{
			VolumetricLightingResolution_Full = 0,
			VolumetricLightingResolution_Half = 1,
			VolumetricLightingResolution_Quarter = 2
		};

	public:
		VolumetricLightingPass(GfxDevice* gfx, uint32 w, uint32 h);
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h)
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
		uint32 width, height;
		VolumetricLightingResolution resolution = VolumetricLightingResolution_Full;
		std::vector<RGResourceName> shadow_textures;
		std::unique_ptr<GfxComputePipelineState> volumetric_lighting_pso;

	private:
		void CreatePSOs();
	};

}