#pragma once
#include "Enums.h"
#include "RendererSettings.h"
#include "BlurPass.h"
#include "CopyToTexturePass.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GPUProfiler;
	class TextureManager;


	class Postprocessor
	{

	public:
		Postprocessor(TextureManager& texture_manager, uint32 width, uint32 height);

		void AddPasses(RenderGraph& rg, PostprocessSettings const& settings);

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

		RGResourceName GetFinalResource() const;
	private:
		TextureManager& texture_manager;
		uint32 width, height;
		std::vector<size_t> cloud_textures;
		RGResourceName final_resource;

		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;

	private:
		void AddCopyHDRPass(RenderGraph& rg);
		void AddVolumetricCloudsPass(RenderGraph& rg);
		void AddSSRPass(RenderGraph& rg);
		//FogPassData const& AddFogPass(RenderGraph& rg, RGTextureRef input, RGTextureSRVRef depth_srv);
	};
}