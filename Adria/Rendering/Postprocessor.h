#pragma once
#include "Enums.h"
#include "RendererSettings.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceRef.h"


namespace adria
{
	class RenderGraph;
	class GPUProfiler;
	class TextureManager;

	struct PostprocessData
	{
		RGTextureRef final_texture;
	};

	class Postprocessor
	{
		struct CopyHDRPassData
		{
			RGTextureRef src_texture;
			RGTextureRef dst_texture;
		};

		struct VolumetricCloudsPassData
		{
			RGTextureRef input;
			RGTextureRef output;
		};

	public:
		Postprocessor(TextureManager& texture_manager, uint32 width, uint32 height);

		PostprocessData const& AddPasses(RenderGraph& rg, PostprocessSettings const& settings,
			RGTextureRef hdr_texture, RGTextureSRVRef depth_srv);

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
	private:
		TextureManager& texture_manager;
		uint32 width, height;
		std::vector<size_t> cloud_textures;
	private:
		CopyHDRPassData const& AddCopyHDRPass(RenderGraph& rg, RGTextureRef hdr_texture);
		VolumetricCloudsPassData const& AddVolumetricCloudsPass(RenderGraph& rg, RGTextureRef input, RGTextureSRVRef depth_srv);
	};
}