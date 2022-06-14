#pragma once
#include "BlurPass.h"
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GraphicsDevice;
	class Texture;

	class HBAOPass
	{
	public:
		static constexpr uint32 NOISE_DIM = 8;
		static constexpr uint32 KERNEL_SIZE = 16;
	public:
		HBAOPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);

		void OnResize(uint32 w, uint32 h);

		void OnSceneInitialized(GraphicsDevice* gfx);
	private:
		uint32 width, height;
		std::unique_ptr<Texture> hbao_random_texture;
		BlurPass blur_pass;
	};

}