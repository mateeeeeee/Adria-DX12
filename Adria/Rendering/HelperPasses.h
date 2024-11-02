#pragma once
#include "RenderGraph/RenderGraphResourceName.h"
#include "Graphics/GfxPipelineStatePermutationsFwd.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	enum class BlendMode : Uint8
	{
		None,
		AlphaBlend,
		AdditiveBlend
	};

	class CopyToTexturePass
	{
	public:
		CopyToTexturePass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~CopyToTexturePass();

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_dst,
			RGResourceName texture_src, BlendMode mode = BlendMode::None);

		void OnResize(Uint32 w, Uint32 h);
		void SetResolution(Uint32 w, Uint32 h);

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> copy_psos;

	private:
		void CreatePSOs();
	};

	class AddTexturesPass
	{
	public:
		AddTexturesPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~AddTexturesPass();

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_dst,
			RGResourceName texture_src1, RGResourceName texture_src2, BlendMode mode = BlendMode::None);

		void OnResize(Uint32 w, Uint32 h);
		void SetResolution(Uint32 w, Uint32 h);

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> add_psos;

	private:
		void CreatePSOs();
	};
}