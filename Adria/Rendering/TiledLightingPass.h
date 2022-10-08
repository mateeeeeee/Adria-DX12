#pragma once
#include <optional>
#include "CopyToTexturePass.h"
#include "AddTexturesPass.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/entity.hpp"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;

	class TiledLightingPass
	{
	public:
		TiledLightingPass(entt::registry& reg, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			copy_to_texture_pass.OnResize(w, h);
			add_textures_pass.OnResize(w, h);
		}

		bool IsVisualized() const { return visualize_tiled; }
		int32 MaxLightsForVisualization() const { return visualize_max_lights; }
	private:
		entt::registry& reg;
		uint32 width, height;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		bool visualize_tiled = false;
		int32 visualize_max_lights = 16;
	};

}