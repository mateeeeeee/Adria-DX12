#pragma once
#include "Enums.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

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
}