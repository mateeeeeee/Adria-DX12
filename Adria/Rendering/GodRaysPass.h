#pragma once
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;
	struct Light;

	class GodRaysPass
	{
	public:
		GodRaysPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, Light const& light);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
	};


}