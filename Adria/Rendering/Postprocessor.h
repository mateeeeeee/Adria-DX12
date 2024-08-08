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
#include "ExponentialHeightFogPass.h"
#include "DepthOfFieldPass.h"
#include "BloomPass.h"
#include "MotionVectorsPass.h"
#include "MotionBlurPass.h"
#include "GodRaysPass.h"
#include "FilmEffectsPass.h"
#include "BokehPass.h"
#include "TAAPass.h"
#include "FSR2Pass.h"
#include "FSR3Pass.h"
#include "XeSSPass.h"
#include "DLSS3Pass.h"
#include "FFXDepthOfFieldPass.h"
#include "FFXCASPass.h"
#include "FFXCACAOPass.h"
#include "FXAAPass.h"
#include "ToneMapPass.h"
#include "SunPass.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "Utilities/Delegate.h"
#include "entt/entity/entity.hpp"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;
	class PostEffect;
	struct Light;
	class RainEvent;
	enum class AmbientOcclusion : uint8;
	enum class Upscaler : uint8;
	enum class Reflections : uint8;
	enum class DepthOfField : uint8;
	enum AntiAliasing : uint8;

	using RenderResolutionChangedDelegate = Delegate<void(uint32, uint32)>;

	class PostProcessor
	{
		DECLARE_EVENT(UpscalerDisabledEvent, PostProcessor, uint32, uint32)
		enum PostEffectType : uint32
		{
			PostEffectType_MotionVectors,
			PostEffectType_LensFlare,
			PostEffectType_Upscaling,
			PostEffectType_Sun,
			PostEffectType_Count
		};

	public:
		PostProcessor(GfxDevice* gfx, entt::registry& reg, uint32 width, uint32 height);
		~PostProcessor();

		void AddAmbientOcclusionPass(RenderGraph& rg);
		void AddPasses(RenderGraph& rg);
		void AddTonemapPass(RenderGraph& rg, RGResourceName input);
		void AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate delegate)
		{
			fsr2_pass.GetRenderResolutionChangedEvent().Add(delegate);
			fsr3_pass.GetRenderResolutionChangedEvent().Add(delegate);
			xess_pass.GetRenderResolutionChangedEvent().Add(delegate);
			dlss3_pass.GetRenderResolutionChangedEvent().Add(delegate);
			upscaler_disabled_event.Add(delegate);
		}


		void OnRainEvent(bool enabled);
		void OnResize(uint32 w, uint32 h);
		void OnRenderResolutionChanged(uint32 w, uint32 h);
		void OnSceneInitialized();

		bool NeedsJitter() const { return HasTAA() || HasUpscaler(); }
		bool NeedsVelocityBuffer() const { return HasTAA() || HasUpscaler() || clouds || motion_blur; }

		void SetFinalResource(RGResourceName name)
		{
			final_resource = name;
		}
		RGResourceName GetFinalResource() const;
		entt::registry& GetRegistry() const { return reg; }

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		uint32 display_width;
		uint32 display_height;
		uint32 render_width;
		uint32 render_height;

		RGResourceName final_resource;
		std::unique_ptr<GfxTexture> history_buffer;

		std::array<std::unique_ptr<PostEffect>, PostEffectType_Count> post_effects;

		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		FFXCACAOPass cacao_pass;
		RayTracedAmbientOcclusionPass rtao_pass;
		AutomaticExposurePass automatic_exposure_pass;
		LensFlarePass lens_flare_pass;
		VolumetricCloudsPass clouds_pass;
		SSRPass ssr_pass;
		RayTracedReflectionsPass rtr_pass;
		ExponentialHeightFogPass fog_pass;
		DepthOfFieldPass dof_pass;
		FFXDepthOfFieldPass ffx_dof_pass;
		BloomPass bloom_pass;
		MotionVectorsPass velocity_buffer_pass;
		MotionBlurPass motion_blur_pass;
		TAAPass taa_pass;
		GodRaysPass god_rays_pass;
		FilmEffectsPass film_effects_pass;
		FSR2Pass fsr2_pass;
		FSR3Pass fsr3_pass;
		XeSSPass xess_pass;
		DLSS3Pass dlss3_pass;
		FFXCASPass cas_pass;
		SunPass sun_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;

		AmbientOcclusion ambient_occlusion;
		Reflections reflections;
		Upscaler upscaler;
		DepthOfField depth_of_field;
		AntiAliasing anti_aliasing;

		bool fog = false;
		bool film_effects = false;
		bool bloom = false;
		bool clouds = true;
		bool motion_blur = false;
		bool automatic_exposure = true;
		bool cas = false;

		UpscalerDisabledEvent upscaler_disabled_event;
		bool ray_tracing_supported = false;

	private:
		void InitializePostEffects();

		RGResourceName AddHDRCopyPass(RenderGraph& rg);

		bool HasUpscaler() const;
		bool HasTAA() const;

		void PostprocessorGUI();
	};
}