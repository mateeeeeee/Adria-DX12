#pragma once
#include "Enums.h"
#include "PipelineState.h"

namespace adria
{
	class GraphicsDevice;

	namespace PSOCache
	{
		void Initialize(GraphicsDevice* gfx);
		void Destroy();
		ComputePipelineState& GetComputePipelineState(EPipelineState);
		GraphicsPipelineState& GetGraphicsPipelineState(EPipelineState);
	};
}