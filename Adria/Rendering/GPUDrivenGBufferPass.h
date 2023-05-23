#pragma once
#include <memory>
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class TextureManager;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;

	class GPUDrivenGBufferPass
	{
		static constexpr uint32 MAX_HZB_MIP_COUNT = 13;

		struct DebugStats
		{
			uint32 num_instances;
			uint32 occluded_instances;
			uint32 visible_instances;
			uint32 processed_meshlets;
			uint32 phase1_candidate_meshlets;
			uint32 phase2_candidate_meshlets;
			uint32 phase1_visible_meshlets;
			uint32 phase2_visible_meshlets;
		};

	public:
		GPUDrivenGBufferPass(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height);

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

		bool occlusion_culling = true;

		std::unique_ptr<GfxBuffer> debug_buffer;
		bool display_debug_stats = false;
		DebugStats debug_stats[3];
	private:
		void InitializeHZB();

		void AddClearCountersPass(RenderGraph& rg);
		void Add1stPhasePasses(RenderGraph& rg);
		void Add2ndPhasePasses(RenderGraph& rg);

		void AddHZBPasses(RenderGraph& rg, bool second_phase = false);
		void AddDebugPass(RenderGraph& rg);

		void CalculateHZBParameters();
		void CreateDebugBuffer();
	};

}