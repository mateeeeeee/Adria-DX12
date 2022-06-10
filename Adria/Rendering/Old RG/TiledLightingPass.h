#pragma once
#include <optional>
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceRef.h"
#include "../../tecs/entity.h"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class DescriptorHeap;
	struct Light;

	struct TiledLightingPassData
	{
		RGTextureUAVRef tiled_uav;
		RGTextureSRVRef tiled_srv;
		RGTextureUAVRef tiled_debug_uav;
		RGTextureSRVRef tiled_debug_srv;
	};

	class TiledLightingPass
	{
	public:
		TiledLightingPass(tecs::registry& reg, uint32 w, uint32 h) : reg(reg), width(w), height(h) {}

		[[maybe_unused]] TiledLightingPassData const& AddPass(RenderGraph& rendergraph,
			RGTextureSRVRef gbuffer_normal_srv,
			RGTextureSRVRef gbuffer_albedo_srv,
			RGTextureSRVRef depth_stencil_srv);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		tecs::registry& reg;
		uint32 width, height;
	};

}