#pragma once
#include <memory>
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class TextureManager;
	class GfxDevice;
	class GfxTexture;

	class GPUDrivenRenderer
	{
		static constexpr uint32 MAX_HZB_MIP_COUNT = 13;

	public:
		GPUDrivenRenderer(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height);

		void Render(RenderGraph& rg);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			InitializeHZB();
		}

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		uint32 width, height;

		std::unique_ptr<GfxTexture> HZB;
		uint32 hzb_mip_count = 0;
		uint32 hzb_width = 0;
		uint32 hzb_height = 0;

	private:
		void InitializeHZB();

		void AddClearCountersPass(RenderGraph& rg);
		void Add1stPhasePasses(RenderGraph& rg);
		void Add2ndPhasePasses(RenderGraph& rg);
		void AddBuildHZBPasses(RenderGraph& rg);

		void CalculateHZBParameters();
	};

}