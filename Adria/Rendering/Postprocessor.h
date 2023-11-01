#pragma once
#include <memory>
#include <d3d12.h>
#include "BlurPass.h"
#include "HelperPasses.h"
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
#include "FSR2Pass.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "Events/Delegate.h"
#include "entt/entity/entity.hpp"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;
	struct Light;

	DECLARE_EVENT(UpscalerDisabledEvent, PostProcessor, uint32, uint32);

	class PostProcessor
	{
		enum class TemporalUpscalerType : uint8
		{
			None,
			FSR2
		};
		enum class Reflections : uint8
		{
			None,
			SSR,
			RTR
		};
		enum AntiAliasing : uint8
		{
			AntiAliasing_None = 0x0,
			AntiAliasing_FXAA = 0x1,
			AntiAliasing_TAA = 0x2
		};

	public:
		PostProcessor(GfxDevice* gfx, entt::registry& reg, uint32 width, uint32 height);
		void AddPasses(RenderGraph& rg);

		template<typename F>
		void AddRenderResolutionChangedCallback(F&& fn)
		{
			fsr2_pass.GetRenderResolutionChangedEvent().Add(std::forward<F>(fn));
			upscaler_disabled_event.Add(std::forward<F>(fn));
		}

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
		RGResourceName GetFinalResource() const;

		bool HasFXAA() const;
		bool HasTAA() const;
		bool HasRTR() const { return reflections == Reflections::RTR; }
		bool HasUpscaler() const { return upscaler != TemporalUpscalerType::None; }
		bool NeedsJitter() const { return HasTAA() || HasUpscaler(); }

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		uint32 display_width;
		uint32 display_height;
		uint32 render_width;
		uint32 render_height;

		RGResourceName final_resource;
		std::unique_ptr<GfxTexture> history_buffer;
		
		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
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
		FSR2Pass fsr2_pass;

		bool ray_tracing_supported = false;
		AntiAliasing anti_aliasing = AntiAliasing_FXAA;
		Reflections reflections = Reflections::SSR;
		TemporalUpscalerType upscaler = TemporalUpscalerType::None;
		bool dof = false;
		bool bokeh = false;
		bool fog = false;
		bool bloom = false;
		bool clouds = true;
		bool motion_blur = false;
		bool automatic_exposure = false;

		UpscalerDisabledEvent upscaler_disabled_event;

	private:
		RGResourceName AddHDRCopyPass(RenderGraph& rg);
		void AddSunPass(RenderGraph& rg, entt::entity sun);

		void PostprocessorGUI();
	};
}