#pragma once

#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	class GenerateMipsPass
	{
	public:
		GenerateMipsPass(uint32 w, uint32 h) : width(w), height(h) {}

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_name);

		void OnResize(uint32 w, uint32 h) { width = w, height = h; }

	private:
		uint32 width; 
		uint32 height;
	};
}