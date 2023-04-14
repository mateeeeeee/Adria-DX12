#pragma once
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class RenderGraph;


	class MotionVectorsPass
	{
	public:
		MotionVectorsPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};
}