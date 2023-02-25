#pragma once
#include <memory>
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class RenderGraph;
	class GraphicsDevice;
	class Buffer;

	class DecalsPass
	{
	public:
		DecalsPass(entt::registry& reg, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);

		void OnResize(uint32 w, uint32 h);

		void OnSceneInitialized(GraphicsDevice* gfx);

	private:
		entt::registry& reg;
		uint32 width, height;
		std::unique_ptr<Buffer>	cube_vb = nullptr;
		std::unique_ptr<Buffer>	cube_ib = nullptr;

	private:
		void CreateCubeBuffers(GraphicsDevice* gfx);
	};
}