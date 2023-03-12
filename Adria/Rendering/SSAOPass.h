#pragma once
#include <DirectXMath.h>
#include <memory>
#include "BlurPass.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;


	class SSAOPass
	{
		struct SSAOParams
		{
			float   ssao_power = 4.0f;
			float   ssao_radius = 1.0f;
		};
	public:
		static constexpr uint32 NOISE_DIM = 8;
		static constexpr uint32 KERNEL_SIZE = 16;
	public:
		SSAOPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized(GfxDevice* gfx);
	private:
		uint32 width, height;
		SSAOParams params{};
		DirectX::XMVECTOR ssao_kernel[KERNEL_SIZE] = {};
		std::unique_ptr<GfxTexture> ssao_random_texture;
		BlurPass blur_pass;
	};

}