#pragma once
#include "BlurPass.h"
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class ComputePipelineState;

	class HBAOPass
	{
		struct HBAOParams
		{
			float   hbao_power = 6.0f;
			float   hbao_radius = 3.0f;
		};

	public:
		static constexpr uint32 NOISE_DIM = 8;

	public:
		HBAOPass(GfxDevice* gfx, uint32 w, uint32 h);
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		HBAOParams params{};
		std::unique_ptr<GfxTexture> hbao_random_texture;
		GfxDescriptor hbao_random_texture_srv;
		BlurPass blur_pass;
		std::unique_ptr<ComputePipelineState> hbao_pso;

	private:
		void CreatePSO();
	};

}