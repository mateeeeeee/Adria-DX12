#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "Utilities/Delegate.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	DECLARE_EVENT(TransparentChangedEvent, TransparentPass, Bool)

	class TransparentPass
	{
	public:
		TransparentPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);
		~TransparentPass();

		void AddPass(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h);
		void GUI();

		TransparentChangedEvent& GetTransparentChangedEvent() { return transparent_changed; }

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> transparent_psos;
		TransparentChangedEvent transparent_changed;

	private:
		void CreatePSOs();
	};
}