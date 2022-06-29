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
	
	class AABBPass
	{
	public:
		AABBPass(entt::registry& reg, uint32 w, uint32 h);
		void AddPass(RenderGraph& rg);

		void OnSceneInitialized(GraphicsDevice* gfx);
		void OnResize(uint32 w, uint32 h);
	private:
		entt::registry& reg;
		uint32 width, height;
		std::unique_ptr<Buffer>	aabb_ib = nullptr;

	private:
		void CreateIndexBuffer(GraphicsDevice* gfx);
	};

}