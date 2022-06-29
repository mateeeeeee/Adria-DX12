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
		ID3D12PipelineState* Get(EPipelineState);
	};
}