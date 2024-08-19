#pragma once
#include "BlurPass.h"
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class GfxStateObject;
	class GfxShaderKey;
	class GfxComputePipelineState;
	class RenderGraph;

	class RayTracedAmbientOcclusionPass
	{
		struct RTAOParams
		{
			float radius = 2.0f;
			float power_log = -1.0f;
			float filter_distance_sigma = 10.0f;
			float filter_depth_sigma = 0.25f;
		};

	public:
		RayTracedAmbientOcclusionPass(GfxDevice* gfx, uint32 width, uint32 height);
		~RayTracedAmbientOcclusionPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(uint32 w, uint32 h);
		bool IsSupported() const;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> ray_traced_ambient_occlusion_so;
		std::unique_ptr<GfxComputePipelineState> rtao_filter_pso;
		uint32 width, height;
		BlurPass blur_pass;

		bool is_supported;
		RTAOParams params{};

	private:
		void CreatePSO();
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}