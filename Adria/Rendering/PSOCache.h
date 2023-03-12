#pragma once
#include "Enums.h"

struct ID3D12PipelineState;

namespace adria
{
	class GfxDevice;

	namespace PSOCache
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();
		ID3D12PipelineState* Get(GfxPipelineStateID);
	};
}