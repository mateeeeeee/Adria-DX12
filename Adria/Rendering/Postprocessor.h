#pragma once
#include "Enums.h"
#include "RendererSettings.h"
#include "Passes/BlurPass.h"
#include "Passes/CopyToTexturePass.h"
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
			RGTextureRTVRef dst_rtv;
		};

		struct VolumetricCloudsPassData
		{
			RGTextureSRVRef output_srv;
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

		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;
	private:
		CopyHDRPassData const& AddCopyHDRPass(RenderGraph& rg, RGTextureRef hdr_texture);
		VolumetricCloudsPassData const& AddVolumetricCloudsPass(RenderGraph& rg, RGTextureSRVRef depth_srv);
	};
}