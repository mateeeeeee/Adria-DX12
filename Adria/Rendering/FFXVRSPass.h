#pragma once
#include "FidelityFX/host/ffx_vrs.h"
#include "Graphics/GfxShadingRate.h"
#include "RenderGraph/RenderGraphResourceName.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;
	class GfxTexture;

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

		FfxInterface*			 ffx_interface;
		FfxVrsContextDescription vrs_context_description;
		FfxVrsContext            vrs_context;
		bool                     context_created = false;

		std::unique_ptr<GfxTexture> vrs_image;

		bool is_supported = false;
		bool  additional_shading_rates_supported = false;
		uint32 shading_rate_image_tile_size;

		//move to console variables and gui later
		float    vrs_threshold = 0.015f;
		float    vrs_motion_factor = 0.01f;

	private:
		void DestroyContext();
		void CreateContext();
	};
}