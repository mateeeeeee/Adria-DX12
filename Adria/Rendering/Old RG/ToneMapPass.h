#pragma once
#include <optional>
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceRef.h"


namespace adria
{
	class RenderGraph;

	struct ToneMapPassData
	{
		RGTextureRef	target;
		RGTextureSRVRef hdr_srv;
	};

	class ToneMapPass
	{
	public:
		ToneMapPass(uint32 w, uint32 h) : width(w), height(h) {}

		ToneMapPassData const& AddPass(RenderGraph& rg, RGTextureRef hdr_texture, std::optional<RGTextureRef> ldr_texture = std::nullopt);

		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
	};
}