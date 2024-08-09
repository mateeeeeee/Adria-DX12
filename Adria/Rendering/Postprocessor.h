#pragma once
#include <memory>
#include <d3d12.h>
#include "BlurPass.h"
#include "HelperPasses.h"
#include "SSAOPass.h"
#include "HBAOPass.h"
#include "RayTracedAmbientOcclusionPass.h"
#include "AutoExposurePass.h"
#include "LensFlarePass.h"
#include "VolumetricCloudsPass.h"
#include "ReflectionPassGroup.h"
#include "DepthOfFieldPassGroup.h"
#include "ExponentialHeightFogPass.h"
#include "BloomPass.h"
#include "MotionVectorsPass.h"
#include "MotionBlurPass.h"
#include "GodRaysPass.h"
#include "FilmEffectsPass.h"
#include "BokehPass.h"
#include "TAAPass.h"
#include "UpscalerPassGroup.h"
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

	enum class AmbientOcclusionType : uint8;
	enum class UpscalerType : uint8;
	enum AntiAliasing : uint8;

	using RenderResolutionChangedDelegate = Delegate<void(uint32, uint32)>;

	class PostProcessor
	{
		enum PostEffectType : uint32
		{
			PostEffectType_MotionVectors,
			PostEffectType_LensFlare,
			PostEffectType_Sun,
			PostEffectType_GodRays,
			PostEffectType_Clouds,
			PostEffectType_Reflections,
			PostEffectType_FilmEffects,
			PostEffectType_Fog,
			PostEffectType_DepthOfField,
			PostEffectType_Upscaler,
			PostEffectType_TAA,
			PostEffectType_MotionBlur,
			PostEffectType_AutoExposure,
			PostEffectType_Bloom,
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
			upscaler_pass.AddRenderResolutionChangedCallback(delegate);
		}

		void OnRainEvent(bool enabled);
		void OnResize(uint32 w, uint32 h);
		void OnRenderResolutionChanged(uint32 w, uint32 h);
		void OnSceneInitialized();

		bool NeedsJitter() const { return HasTAA() || HasUpscaler(); }
		bool NeedsVelocityBuffer() const { return HasTAA() || HasUpscaler() || clouds_pass.IsEnabled(this) || motion_blur_pass.IsEnabled(this); }
		bool HasUpscaler() const;
		bool HasTAA() const;

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
		std::array<std::unique_ptr<PostEffect>, PostEffectType_Count> post_effects;

		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		FFXCACAOPass cacao_pass;
		RayTracedAmbientOcclusionPass rtao_pass;
		AutoExposurePass automatic_exposure_pass;
		LensFlarePass lens_flare_pass;
		VolumetricCloudsPass clouds_pass;
		ReflectionPassGroup reflections_pass;
		DepthOfFieldPassGroup depth_of_field_pass;
		ExponentialHeightFogPass fog_pass;
		BloomPass bloom_pass;
		MotionVectorsPass velocity_buffer_pass;
		MotionBlurPass motion_blur_pass;
		TAAPass taa_pass;
		GodRaysPass god_rays_pass;
		FilmEffectsPass film_effects_pass;
		UpscalerPassGroup upscaler_pass;
		FFXCASPass cas_pass;
		SunPass sun_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;

		AmbientOcclusionType ambient_occlusion;
		bool fxaa = true;
		bool cas = false;
		bool ray_tracing_supported = false;

	private:
		void InitializePostEffects();

		RGResourceName AddHDRCopyPass(RenderGraph& rg);


		void PostprocessorGUI();
	};
}