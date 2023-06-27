#pragma once
#include "HelperPasses.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/entity.hpp"

namespace adria
{
	class RenderGraph;

	class TiledDeferredLightingPass
	{
	public:
		TiledDeferredLightingPass(entt::registry& reg, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			copy_to_texture_pass.OnResize(w, h);
			add_textures_pass.OnResize(w, h);
		}
	private:
		entt::registry& reg;
		uint32 width, height;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		bool visualize_tiled = false;
		int32 visualize_max_lights = 16;
	};

}