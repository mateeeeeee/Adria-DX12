#pragma once
#include "Enums.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;


	class ToneMapPass
	{
		struct TonemapParams
		{
			EToneMap tone_map_op = EToneMap::Reinhard;
			float32 tonemap_exposure = 1.0f;
		};
	public:
		ToneMapPass(uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src, RGResourceName output);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
		TonemapParams params;

	private:
		void GUI();
	};
}