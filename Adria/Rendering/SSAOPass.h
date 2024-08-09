#pragma once
#include "BlurPass.h"
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class GfxDevice;
	class GfxTexture;
	class GfxComputePipelineState;
	class RenderGraph;

	class SSAOPass
	{
	public:
		static constexpr uint32 NOISE_DIM = 8;
		static constexpr uint32 KERNEL_SIZE = 16;
	public:
		SSAOPass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		Vector4 ssao_kernel[KERNEL_SIZE] = {};
		std::unique_ptr<GfxComputePipelineState> ssao_pso;
		std::unique_ptr<GfxTexture> ssao_random_texture;
		GfxDescriptor ssao_random_texture_srv;
		BlurPass blur_pass;

	private:
		void CreatePSO();
	};

}