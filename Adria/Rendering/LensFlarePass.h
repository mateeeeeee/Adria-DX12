#pragma once
#include <vector>
#include "TextureHandle.h"
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	struct Light;

	class LensFlarePass
	{
	public:
		LensFlarePass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, Light const& light);
		void AddPass2(RenderGraph& rendergraph, Light const& light);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
	private:
		uint32 width, height;
		std::vector<TextureHandle> lens_flare_textures;
	};

}