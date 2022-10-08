#pragma once
#include <vector>
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GraphicsDevice;
	class TextureManager;
	struct Light;

	class LensFlarePass
	{
	public:
		LensFlarePass(TextureManager& texture_manager, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, Light const& light);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized(GraphicsDevice* gfx);
	private:
		TextureManager& texture_manager;
		uint32 width, height;
		std::vector<size_t> lens_flare_textures;
	};

}