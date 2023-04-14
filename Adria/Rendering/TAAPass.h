#pragma once
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	class TAAPass
	{
	public:
		TAAPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input, RGResourceName history);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};

}