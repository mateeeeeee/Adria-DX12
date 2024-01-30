#pragma once
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class RenderGraph;
	class RainBlockerMapPass
	{
		static constexpr uint32 BLOCKER_DIM = 256;
	public:
		RainBlockerMapPass(entt::registry& reg, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);

		Matrix const& GetViewProjection() const { return view_projection; }

	private:
		entt::registry& reg;
		uint32 width, height;
		Matrix view_projection;
	};
}