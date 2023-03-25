#pragma once
#include "../Core/CoreTypes.h"
#include "../RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class SSRPass
	{
		struct SSRParameters
		{
			float ssr_ray_step = 1.60f;
			float ssr_ray_hit_threshold = 2.00f;
		};
	public:
		SSRPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
		SSRParameters params{};
	};

}