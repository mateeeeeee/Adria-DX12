#pragma once
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"

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
		float ambient_color[3] = { 6.0f / 255.0f, 6.0f / 255.0f, 6.0f / 255.0f };
		bool ibl = false;
	};

}