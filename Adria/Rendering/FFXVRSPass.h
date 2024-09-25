#pragma once
#include "FidelityFX/host/ffx_vrs.h"
#include "Graphics/GfxShadingRate.h"
#include "RenderGraph/RenderGraphResourceName.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FFXVRSPass
	{
	public:
		FFXVRSPass(GfxDevice* gfx, uint32 w, uint32 h);
		~FFXVRSPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(uint32 w, uint32 h);

	private:
		char name_version[16] = {};
		GfxDevice* gfx;
		uint32 width, height;

		FfxVrsContextDescription vrs_context_description;
		FfxVrsContext            vrs_context;
		bool                     context_created = false;
	};
}