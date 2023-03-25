#pragma once
#include "../Core/CoreTypes.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	class FXAAPass
	{
	public:
		FXAAPass(uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, RGResourceName input);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
	};
}


