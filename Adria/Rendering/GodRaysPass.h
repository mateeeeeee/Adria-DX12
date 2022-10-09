#pragma once
#include "Enums.h"
#include "GenerateMipsPass.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;
	struct Light;

	class GodRaysPass
	{
	public:
		GodRaysPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, Light const& light);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
	};


}