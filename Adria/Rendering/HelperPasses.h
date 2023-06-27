#pragma once

#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

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
			RGResourceName render_target,
			RGResourceName texture_src, BlendMode mode = BlendMode::None);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		uint32 width, height;
	};

	class AddTexturesPass
	{
	public:
		AddTexturesPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph,
			RGResourceName render_target,
			RGResourceName texture1, RGResourceName texture2, BlendMode mode = BlendMode::None);

		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};
}