#pragma once
#include <optional>

#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	class BlurPass
	{
	public:
		BlurPass(uint32 w, uint32 h) : width(w), height(h) {}

		void AddPass(RenderGraph& rendergraph,
			RGResourceName src_texture, RGResourceName blurred_texture, char const* pass_name = "");

		void OnResize(uint32 w, uint32 h);
		void SetResolution(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};
}