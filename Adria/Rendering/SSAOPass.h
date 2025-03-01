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
		static constexpr Uint32 NOISE_DIM = 8;
		static constexpr Uint32 KERNEL_SIZE = 16;
	public:
		SSAOPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~SSAOPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		Vector4 ssao_kernel[KERNEL_SIZE] = {};
		std::unique_ptr<GfxComputePipelineState> ssao_pso;
		std::unique_ptr<GfxTexture> ssao_random_texture;
		GfxDescriptor ssao_random_texture_srv;
		BlurPass blur_pass;

	private:
		void CreatePSO();
	};

}