#pragma once
#include <DirectXMath.h>
#include <memory>
#include "BlurPass.h"
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class GfxDevice;
	class GfxTexture;
	class ComputePipelineState;
	class RenderGraph;

	class SSAOPass
	{
		struct SSAOParams
		{
			float   ssao_power = 1.5f;
			float   ssao_radius = 1.0f;
		};
		enum SSAOResolution
		{
			SSAOResolution_Full = 0,
			SSAOResolution_Half = 1,
			SSAOResolution_Quarter = 2
		};

	public:
		static constexpr uint32 NOISE_DIM = 8;
		static constexpr uint32 KERNEL_SIZE = 16;

	public:
		SSAOPass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		SSAOParams params{};
		SSAOResolution resolution = SSAOResolution_Half;

		DirectX::XMVECTOR ssao_kernel[KERNEL_SIZE] = {};
		std::unique_ptr<GfxTexture> ssao_random_texture;
		GfxDescriptor ssao_random_texture_srv;
		BlurPass blur_pass;

		std::unique_ptr<ComputePipelineState> ssao_pso;

	private:
		void CreatePSO();
	};

}