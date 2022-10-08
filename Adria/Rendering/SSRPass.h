#pragma once
#include <vector>
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;
	class GraphicsDevice;
	class TextureManager;

	struct SSRParameters
	{
		float32 ssr_ray_step = 1.60f;
		float32 ssr_ray_hit_threshold = 2.00f;
	};

	class SSRPass
	{
	public:
		SSRPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

		SSRParameters const& GetParams() const { return params; }

	private:
		uint32 width, height;
		SSRParameters params{};
	};

}