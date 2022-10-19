#pragma once
#include <memory>
#include <d3d12.h>
#include <wrl.h>
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
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/entity.hpp"


namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class GraphicsDevice;
	class GPUProfiler;
	class TextureManager;
	class Texture;
	class Buffer;
	struct Light;

	class Postprocessor
	{
	public:
		Postprocessor(entt::registry& reg, TextureManager& texture_manager, uint32 width, uint32 height);
		void AddPasses(RenderGraph& rg, PostprocessSettings const& settings);

		void OnResize(GraphicsDevice* gfx, uint32 w, uint32 h);
		void OnSceneInitialized(GraphicsDevice* gfx);
		RGResourceName GetFinalResource() const;

		CloudParameters const& GetCloudParams() const { return clouds_pass.GetParams(); }
		DoFParameters   const& GetDoFParams() const { return dof_pass.GetParams(); }
		BloomParameters const& GetBloomParams() const { return bloom_pass.GetParams(); }
		BokehParameters const& GetBokehParams() const { return bokeh_pass.GetParams(); }

	private:
		entt::registry& reg;
		TextureManager& texture_manager;
		uint32 width, height;
		PostprocessSettings settings;

		RGResourceName final_resource;
		std::unique_ptr<Texture> history_buffer;
		
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