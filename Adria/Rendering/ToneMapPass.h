#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class RenderGraph;

	struct ToneMapPassData
	{
		RGRenderTargetId target;
		RGTextureReadOnlyId hdr_srv;
	};

	class ToneMapPass
	{
	public:
		ToneMapPass(uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, bool render_to_backbuffer);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
	};
}