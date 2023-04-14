#pragma once
#include <memory>
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxBuffer;
	
	class AABBPass
	{
	public:
		AABBPass(entt::registry& reg, uint32 w, uint32 h);
		void AddPass(RenderGraph& rg);

		void OnSceneInitialized(GfxDevice* gfx);
		void OnResize(uint32 w, uint32 h);
	private:
		entt::registry& reg;
		uint32 width, height;
		std::unique_ptr<GfxBuffer>	aabb_ib = nullptr;

	private:
		void CreateIndexBuffer(GfxDevice* gfx);
	};

}