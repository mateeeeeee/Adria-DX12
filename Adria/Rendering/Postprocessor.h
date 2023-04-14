#pragma once
#include <memory>
#include <d3d12.h>
#include "Enums.h"
#include "RendererSettings.h"
#include "BlurPass.h"
#include "CopyToTexturePass.h"
#include "AddTexturesPass.h"
#include "GenerateMipsPass.h"
#include "AutomaticExposurePass.h"
#include "LensFlarePass.h"
#include "VolumetricCloudsPass.h"
#include "SSRPass.h"
#include "FogPass.h"
#include "DepthOfFieldPass.h"
#include "BloomPass.h"
#include "MotionVectorsPass.h"
#include "MotionBlurPass.h"
#include "TAAPass.h"
#include "GodRaysPass.h"
#include "BokehPass.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/entity.hpp"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;
	struct Light;

	class Postprocessor
	{
	public:
		Postprocessor(entt::registry& reg, uint32 width, uint32 height);
		void AddPasses(RenderGraph& rg, PostprocessSettings const& settings);

		void OnResize(GfxDevice* gfx, uint32 w, uint32 h);
		void OnSceneInitialized(GfxDevice* gfx);
		RGResourceName GetFinalResource() const;

	private:
		entt::registry& reg;
		uint32 width, height;
		PostprocessSettings settings;

		RGResourceName final_resource;
		std::unique_ptr<GfxTexture> history_buffer;
		
		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		GenerateMipsPass generate_mips_pass;
		AutomaticExposurePass automatic_exposure_pass;
		LensFlarePass lens_flare_pass;
		VolumetricCloudsPass clouds_pass;
		SSRPass ssr_pass;
		FogPass fog_pass;
		DepthOfFieldPass dof_pass;
		BloomPass bloom_pass;
		MotionVectorsPass velocity_buffer_pass;
		MotionBlurPass motion_blur_pass;
		TAAPass taa_pass;
		GodRaysPass god_rays_pass;
		BokehPass bokeh_pass;
	private:
		RGResourceName AddHDRCopyPass(RenderGraph& rg);
		void AddHistoryCopyPass(RenderGraph& rg);
		void AddSunPass(RenderGraph& rg, entt::entity sun);
	};
}