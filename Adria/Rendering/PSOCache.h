#pragma once
#include "Enums.h"



namespace adria
{
	class GfxDevice;
	class GfxPipelineState;

	namespace PSOCache
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();
		GfxPipelineState* Get(GfxPipelineStateID);
	};
}