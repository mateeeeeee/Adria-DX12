#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	class ToneMapPass
	{
	public:
		ToneMapPass(uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src, bool render_to_backbuffer);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
	};
}