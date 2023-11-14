#pragma once
#include "FidelityFX/host/ffx_dof.h" 
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FFXDepthOfFieldPass
	{
		
	public:
		FFXDepthOfFieldPass(GfxDevice* gfx, FfxInterface& ffx_interface, uint32 w, uint32 h);
		~FFXDepthOfFieldPass();

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		char name_version[20] = {};
		GfxDevice* gfx;
		uint32 width, height;

		FfxDofContextDescription ffx_dof_context_desc{};
		FfxDofContext            ffx_dof_context{};

		float aperture = 0.01f;
		float focus_dist = 400.0f;
		float sensor_size = 0.02f;
		float coc_limit = 0.01f; 
		int32 quality = 10;
		bool  enable_ring_merge = false;

	private:
		void CreateContext();
		void DestroyContext();
	};

}