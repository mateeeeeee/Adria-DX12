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
		struct VRSFeatureInfo
		{
			bool                   additional_shading_rates_supported = false;
			GfxShadingRate         shading_rates[MAX_SHADING_RATES];
			uint32                 number_of_shading_rates = 0;
			GfxShadingRateCombiner combiners;
			uint32				   shading_rate_image_tile_size;
		};

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

		VRSFeatureInfo vrs_feature_info;
		std::unique_ptr<GfxTexture> vrs_image;

	private:
		void FillFeatureInfo();
		void CreateVRSImage();

		void DestroyContext();
		void CreateContext();
	};
}