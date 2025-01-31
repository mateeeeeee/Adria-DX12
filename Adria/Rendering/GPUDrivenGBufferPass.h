#pragma once
#include "RenderGraph/RenderGraphResourceId.h"
#include "Graphics/GfxMacros.h"
#include "Graphics/GfxPipelineStatePermutationsFwd.h"


namespace adria
{
	class RenderGraph;
	class TextureManager;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;
	enum class RendererOutput : Uint32;

	class GPUDrivenGBufferPass
	{
		static constexpr Uint32 MAX_HZB_MIP_COUNT = 13;
		struct DebugStats
		{
			Uint32 num_instances;
			Uint32 occluded_instances;
			Uint32 visible_instances;
			Uint32 processed_meshlets;
			Uint32 phase1_candidate_meshlets;
			Uint32 phase2_candidate_meshlets;
			Uint32 phase1_visible_meshlets;
			Uint32 phase2_visible_meshlets;
		};


	public:
		GPUDrivenGBufferPass(entt::registry& reg, GfxDevice* gfx, Uint32 width, Uint32 height);
		~GPUDrivenGBufferPass();

		void AddPasses(RenderGraph& rg);
		void GUI();

		Bool IsSupported() const;
		Bool IsEnabled() const;

		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
			InitializeHZB();
		}

		void OnRainEvent(Bool enabled)
		{
			rain_active = enabled;
		}
		void OnRendererOutputChanged(RendererOutput renderer_output);

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		Uint32 width, height;

		std::unique_ptr<GfxTexture> HZB;
		Uint32 hzb_mip_count = 0;
		Uint32 hzb_width = 0;
		Uint32 hzb_height = 0;

		Bool occlusion_culling = true;

		std::unique_ptr<GfxBuffer> debug_buffer;
		Bool display_debug_stats = false;
		DebugStats debug_stats[GFX_BACKBUFFER_COUNT] = {};

		Bool rain_active = false;
		Bool debug_mipmaps = false;
		Bool triangle_overdraw = false;
		std::unique_ptr<GfxMeshShaderPipelineStatePermutations> draw_psos;
		std::unique_ptr<GfxComputePipelineStatePermutations>	cull_meshlets_psos;
		std::unique_ptr<GfxComputePipelineStatePermutations>	cull_instances_psos;
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

		void AddHZBPasses(RenderGraph& rg, Bool second_phase = false);
		void AddDebugPass(RenderGraph& rg);

		void CalculateHZBParameters();
		void CreateDebugBuffer();
	};

}