#pragma once
#include <memory>
#include <d3d12.h>
#include "BlurPass.h"
#include "HelperPasses.h"
#include "SSAOPass.h"
#include "HBAOPass.h"
#include "RayTracedAmbientOcclusionPass.h"
#include "AutomaticExposurePass.h"
#include "LensFlarePass.h"
#include "VolumetricCloudsPass.h"
#include "SSRPass.h"
#include "RayTracedReflectionsPass.h"
#include "FogPass.h"
#include "DepthOfFieldPass.h"
#include "BloomPass.h"
#include "MotionVectorsPass.h"
#include "MotionBlurPass.h"
#include "GodRaysPass.h"
#include "BokehPass.h"
#include "TAAPass.h"
#include "FSR2Pass.h"
#include "XeSSPass.h"
#include "DLSS3Pass.h"
#include "FFXDepthOfFieldPass.h"
#include "FFXCASPass.h"
#include "FFXCACAOPass.h"
#include "FidelityFXManager.h"
#include "FXAAPass.h"
#include "ToneMapPass.h"
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


	class PostProcessor
	{
		enum class AmbientOcclusion : uint8
		{
			None,
			SSAO,
			HBAO,
			CACAO,
			RTAO
		};
		enum class UpscalerType : uint8
		{
			None,
			FSR2,
			XeSS,
			DLSS3
		};
		enum class Reflections : uint8
		{
			None,
			SSR,
			RTR
		};
		enum class DepthOfField : uint8
		{
			None,
			Simple,
			FFX
		};
		enum AntiAliasing : uint8
		{
			AntiAliasing_None = 0x0,
			AntiAliasing_FXAA = 0x1,
			AntiAliasing_TAA = 0x2
		};

		DECLARE_EVENT(UpscalerDisabledEvent, PostProcessor, uint32, uint32);

	public:
		PostProcessor(GfxDevice* gfx, entt::registry& reg, uint32 width, uint32 height);

		void AddAmbientOcclusionPass(RenderGraph& rg);
		void AddPasses(RenderGraph& rg);
		void AddTonemapPass(RenderGraph& rg, RGResourceName input);

		template<typename T, typename... Args>
		void AddRenderResolutionChangedCallback(void(T::* mem_pfn)(Args...), T& instance)
		{
			fsr2_pass.GetRenderResolutionChangedEvent().AddMember(mem_pfn, instance);
			xess_pass.GetRenderResolutionChangedEvent().AddMember(mem_pfn, instance);
			dlss3_pass.GetRenderResolutionChangedEvent().AddMember(mem_pfn, instance);
			upscaler_disabled_event.AddMember(mem_pfn, instance);
		}

		RGResourceName GetFinalResource() const;

		void OnResize(uint32 w, uint32 h);
		void OnRenderResolutionChanged(uint32 w, uint32 h);
		void OnSceneInitialized();

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

		FidelityFXManager ffx_manager;

		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		FFXCACAOPass& cacao_pass;
		RayTracedAmbientOcclusionPass rtao_pass;
		AutomaticExposurePass automatic_exposure_pass;
		LensFlarePass lens_flare_pass;
		VolumetricCloudsPass clouds_pass;
		SSRPass ssr_pass;
		RayTracedReflectionsPass rtr_pass;
		FogPass fog_pass;
		DepthOfFieldPass dof_pass;
		FFXDepthOfFieldPass& ffx_dof_pass;
		BloomPass bloom_pass;
		MotionVectorsPass velocity_buffer_pass;
		MotionBlurPass motion_blur_pass;
		TAAPass taa_pass;
		GodRaysPass god_rays_pass;
		FSR2Pass& fsr2_pass;
		XeSSPass xess_pass;
		DLSS3Pass dlss3_pass;
		FFXCASPass& cas_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;

		bool ray_tracing_supported = false;

		AmbientOcclusion ambient_occlusion = AmbientOcclusion::SSAO;
		Reflections reflections = Reflections::None;
		UpscalerType upscaler = UpscalerType::None;
		DepthOfField dof = DepthOfField::None;
		AntiAliasing anti_aliasing = AntiAliasing_FXAA;

		bool fog = false;
		bool bloom = false;
		bool clouds = true;
		bool motion_blur = false;
		bool automatic_exposure = true;
		bool cas = false;

		UpscalerDisabledEvent upscaler_disabled_event;

	private:
		RGResourceName AddHDRCopyPass(RenderGraph& rg);
		void AddSunPass(RenderGraph& rg, entt::entity sun);

		bool NeedsVelocityBuffer() const { return HasUpscaler() || HasTAA() || clouds || motion_blur; }
		bool HasUpscaler() const { return upscaler != UpscalerType::None; }
		bool HasTAA() const;

		void PostprocessorGUI();
	};
}