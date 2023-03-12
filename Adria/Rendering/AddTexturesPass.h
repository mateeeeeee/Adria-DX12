#pragma once
#include "Enums.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;

	class AddTexturesPass
	{
	public:
		AddTexturesPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph,
			RGResourceName render_target,
			RGResourceName texture1, RGResourceName texture2, BlendMode mode = BlendMode::None);

		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
	};

}