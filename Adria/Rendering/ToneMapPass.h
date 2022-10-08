#pragma once
#include "Enums.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	struct TonemapParams
	{
		EToneMap tone_map_op = EToneMap::Reinhard;
		float32 tonemap_exposure = 1.0f;
	};

	class ToneMapPass
	{
	public:
		ToneMapPass(uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src, bool render_to_backbuffer);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src, RGResourceName fxaa_input);
		void OnResize(uint32 w, uint32 h);

		TonemapParams const& GetParams() const { return params; }
	private:
		uint32 width, height;
		TonemapParams params;

	private:

		void GUI();
	};
}