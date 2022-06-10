#pragma once
#include <optional>
#include "../Enums.h"
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceRef.h"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class DescriptorHeap;
	struct Light;

	struct CopyToTexturePassData
	{
		RGTextureRTVRef render_target;
		RGTextureSRVRef texture_srv;
	};

	class CopyToTexturePass
	{
	public:
		CopyToTexturePass(uint32 w, uint32 h) : reg(reg), width(w), height(h) {}

		[[maybe_unused]] CopyToTexturePassData const& AddPass(RenderGraph& rendergraph,
			RGTextureRTVRef render_target,
			RGTextureSRVRef texture_srv, EBlendMode mode = EBlendMode::None);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		tecs::registry& reg;
		uint32 width, height;
	};
}