#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	class MotionBlurPass
	{
	public:
		MotionBlurPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};

}