#pragma once
#include "Enums.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class DepthOfFieldPass
	{
		struct DoFParameters
		{
			float32 dof_near_blur = 0.0f;
			float32 dof_near = 200.0f;
			float32 dof_far = 400.0f;
			float32 dof_far_blur = 600.0f;
		};
	public:
		DepthOfFieldPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
		DoFParameters params{};
	};

}