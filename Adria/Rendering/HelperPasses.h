#pragma once
#include "RenderGraph/RenderGraphResourceName.h"
#include "Graphics/GfxTexture.h"

namespace adria
{
	class RenderGraph;

	enum class BlendMode : uint8
	{
		None,
		AlphaBlend,
		AdditiveBlend
	};

	class CopyToTexturePass
	{
	public:
		CopyToTexturePass(uint32 w, uint32 h) : width(w), height(h) {}

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_dst,
			RGResourceName texture_src, BlendMode mode = BlendMode::None);

		void OnResize(uint32 w, uint32 h);
		void SetResolution(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};

	class AddTexturesPass
	{
	public:
		AddTexturesPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_dst,
			RGResourceName texture_src1, RGResourceName texture_src2, BlendMode mode = BlendMode::None);

		void OnResize(uint32 w, uint32 h);
		void SetResolution(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};

	class TextureChannelMappingPass
	{
	public:
		TextureChannelMappingPass() = default;

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_dst,
			RGResourceName texture_src, GfxTextureChannelMapping);

	};
}