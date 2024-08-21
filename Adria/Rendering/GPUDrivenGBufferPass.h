#pragma once
#include "RenderGraph/RenderGraphResourceId.h"
#include "Graphics/GfxDefines.h"
#include "Graphics/GfxPipelineStatePermutationsFwd.h"


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
		~GPUDrivenGBufferPass();

		void AddPasses(RenderGraph& rg);
		void GUI();

		bool IsSupported() const;
		bool IsEnabled() const;

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			InitializeHZB();
		}

		void OnRainEvent(bool enabled)
		{
			rain_active = enabled;
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
		DebugStats debug_stats[GFX_BACKBUFFER_COUNT] = {};

		bool rain_active = false;
		std::unique_ptr<GfxMeshShaderPipelineStatePermutations> draw_psos;
		std::unique_ptr<GfxComputePipelineStatePermutations>	  cull_meshlets_psos;
		std::unique_ptr<GfxComputePipelineStatePermutations>	  cull_instances_psos;
		std::unique_ptr<GfxComputePipelineStatePermutations>    build_meshlet_cull_args_psos;
		std::unique_ptr<GfxComputePipelineStatePermutations>    build_meshlet_draw_args_psos;
		std::unique_ptr<GfxComputePipelineState> clear_counters_pso;
		std::unique_ptr<GfxComputePipelineState> build_instance_cull_args_pso;
		std::unique_ptr<GfxComputePipelineState> initialize_hzb_pso;
		std::unique_ptr<GfxComputePipelineState> hzb_mips_pso;

	private:
		void CreatePSOs();
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