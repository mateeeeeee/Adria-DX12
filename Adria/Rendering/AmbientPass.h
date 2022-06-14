#pragma once
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class RenderGraph;

	class AmbientPass
	{
	public:
		AmbientPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);

		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};

}