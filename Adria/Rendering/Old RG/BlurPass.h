#pragma once
#include <optional>
#include "../Enums.h"
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceRef.h"

namespace adria
{
	class RenderGraph;

	struct BlurPassData
	{
		RGTextureSRVRef src_srv;
		RGTextureSRVRef intermediate_srv;
		RGTextureUAVRef intermediate_uav;
		RGTextureSRVRef final_srv;
		RGTextureUAVRef final_uav;
	};

	class BlurPass
	{
	public:
		BlurPass(uint32 w, uint32 h) :width(w), height(h) {}

		[[maybe_unused]] BlurPassData const& AddPass(RenderGraph& rendergraph,
			RGTextureSRVRef texture_srv, char const* name = "");

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		uint32 width, height;
	};
}