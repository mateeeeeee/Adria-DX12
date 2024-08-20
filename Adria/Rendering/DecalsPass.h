#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxBuffer;

	class DecalsPass
	{
	public:
		DecalsPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h);
		~DecalsPass();

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxBuffer>	cube_vb = nullptr;
		std::unique_ptr<GfxBuffer>	cube_ib = nullptr;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> decal_psos;

	private:
		void CreatePSOs();
		void CreateCubeBuffers();
	};
}