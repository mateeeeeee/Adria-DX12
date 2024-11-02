#pragma once
#include "SSAOPass.h"
#include "HBAOPass.h"
#include "RayTracedAmbientOcclusionPass.h"
#include "FFXCACAOPass.h"
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

	enum AmbientOcclusionType : Uint8;
	enum class UpscalerType : Uint8;
	enum AntiAliasing : Uint8;

	using RenderResolutionChangedDelegate = Delegate<void(Uint32, Uint32)>;
	class PostProcessor
	{
		enum PostEffectType : Uint32
		{
			PostEffectType_MotionVectors,
			PostEffectType_LensFlare,
			PostEffectType_Sun,
			PostEffectType_GodRays,
			PostEffectType_Clouds,
			PostEffectType_Reflection,
			PostEffectType_FilmEffects,
			PostEffectType_Fog,
			PostEffectType_DepthOfField,
			PostEffectType_Upscaler,
			PostEffectType_TAA,
			PostEffectType_MotionBlur,
			PostEffectType_AutoExposure,
			PostEffectType_Bloom,
			PostEffectType_CAS,
			PostEffectType_ToneMap,
			PostEffectType_VRS,
			PostEffectType_FXAA,
			PostEffectType_Count
		};

	public:
		PostProcessor(GfxDevice* gfx, entt::registry& reg, Uint32 width, Uint32 height);
		~PostProcessor();

		void AddAmbientOcclusionPass(RenderGraph& rg);
		void AddPasses(RenderGraph& rg);
		void AddTonemapPass(RenderGraph& rg, RGResourceName input);
		void AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate delegate);
		void GUI();

		void OnRainEvent(bool enabled);
		void OnResize(Uint32 w, Uint32 h);
		void OnRenderResolutionChanged(Uint32 w, Uint32 h);
		void OnSceneInitialized();

		bool NeedsJitter() const { return HasTAA() || HasUpscaler(); }
		bool NeedsVelocityBuffer() const;
		bool NeedsHistoryBuffer() const;
		bool HasUpscaler() const;
		bool HasTAA() const;
		bool HasFXAA() const;
		bool IsPathTracing() const;

		void SetFinalResource(RGResourceName name)
		{
			final_resource = name;
		}
		RGResourceName GetFinalResource() const;
		entt::registry& GetRegistry() const { return reg; }

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		Uint32 display_width;
		Uint32 display_height;
		Uint32 render_width;
		Uint32 render_height;
		bool ray_tracing_supported = false;
		bool is_path_tracing_path = false;

		RGResourceName final_resource;

		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		FFXCACAOPass cacao_pass;
		RayTracedAmbientOcclusionPass rtao_pass;

		std::array<std::unique_ptr<PostEffect>, PostEffectType_Count> post_effects;
		std::unique_ptr<GfxTexture> history_buffer;

	private:
		void InitializePostEffects();
		void CreateHistoryBuffer();

		template<typename PostEffectT> requires std::is_base_of_v<PostEffect, PostEffectT>
		PostEffectT* GetPostEffect() const;
	};


}