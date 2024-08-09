#include "PostProcessor.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "RainPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{
	static TAutoConsoleVariable<int>  cvar_ambient_occlusion("r.AmbientOcclusion", 1, "0 - No AO, 1 - SSAO, 2 - HBAO, 3 - CACAO, 4 - RTAO");
	static TAutoConsoleVariable<bool> cvar_fxaa("r.FXAA", true, "Enable or Disable FXAA");
	static TAutoConsoleVariable<bool> cvar_bloom("r.Bloom", false, "Enable or Disable Bloom");
	static TAutoConsoleVariable<bool> cvar_motion_blur("r.MotionBlur", false, "Enable or Disable Motion Blur");
	static TAutoConsoleVariable<bool> cvar_autoexposure("r.AutoExposure", false, "Enable or Disable Auto Exposure");
	static TAutoConsoleVariable<bool> cvar_cas("r.CAS", false, "Enable or Disable Contrast-Adaptive Sharpening, TAA must be enabled");
	
	enum class AmbientOcclusionType : uint8
	{
		None,
		SSAO,
		HBAO,
		CACAO,
		RTAO
	};

	PostProcessor::PostProcessor(GfxDevice* gfx, entt::registry& reg, uint32 width, uint32 height)
		: gfx(gfx), reg(reg), display_width(width), display_height(height), render_width(width), render_height(height),
		ssao_pass(gfx, width, height), hbao_pass(gfx, width, height), rtao_pass(gfx, width, height), 
		film_effects_pass(gfx, width, height), automatic_exposure_pass(gfx, width, height), lens_flare_pass(gfx, width, height), 
		clouds_pass(gfx, width, height), reflections_pass(gfx, width, height),
		fog_pass(gfx, width, height), depth_of_field_pass(gfx, width, height), bloom_pass(gfx, width, height), 
		velocity_buffer_pass(gfx, width, height), motion_blur_pass(gfx, width, height), taa_pass(gfx, width, height),
		god_rays_pass(gfx, width, height), upscaler_pass(gfx, width, height),
		tonemap_pass(gfx, width, height), fxaa_pass(gfx, width, height), sun_pass(gfx, width, height),
		cas_pass(gfx, width, height), cacao_pass(gfx, width, height),
		ambient_occlusion(AmbientOcclusionType::SSAO)
	{
		ray_tracing_supported = gfx->GetCapabilities().SupportsRayTracing();
		upscaler_pass.AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate::CreateMember(&PostProcessor::OnRenderResolutionChanged, *this));
		//cvars
		{
			cvar_ambient_occlusion->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) { ambient_occlusion = static_cast<AmbientOcclusionType>(cvar->GetInt()); }));
			cvar_fxaa->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar)
				{
					fxaa = cvar->GetBool();
				}));
			cvar_bloom->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar){ bloom = cvar->GetBool(); }));
			cvar_motion_blur->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar){ motion_blur = cvar->GetBool(); }));
			cvar_autoexposure->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar){ automatic_exposure = cvar->GetBool(); }));
			cvar_cas->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar){ cas = cvar->GetBool(); }));
		}

		//InitializePostEffects();
	}

	PostProcessor::~PostProcessor()
	{
	}

	void PostProcessor::OnRainEvent(bool enabled)
	{
		clouds_pass.OnRainEvent(enabled);
	}

	void PostProcessor::AddAmbientOcclusionPass(RenderGraph& rg)
	{
		switch (ambient_occlusion)
		{
		case AmbientOcclusionType::SSAO:  ssao_pass.AddPass(rg); break;
		case AmbientOcclusionType::HBAO:  hbao_pass.AddPass(rg); break;
		case AmbientOcclusionType::CACAO: cacao_pass.AddPass(rg); break;
		case AmbientOcclusionType::RTAO:  rtao_pass.AddPass(rg); break;
		}
	}

	void PostProcessor::AddPasses(RenderGraph& rg)
	{
		PostprocessorGUI();
		final_resource = AddHDRCopyPass(rg);

		/*
		for(auto& post_effect : post_effects)
		{
			if(post_effect.IsEnabled(this)) post_effect.AddPass(rg, this);
		}
		*/

		if (velocity_buffer_pass.IsEnabled(this))
		{
			velocity_buffer_pass.AddPass(rg, this);
		}
		lens_flare_pass.AddPass(rg, this);
		sun_pass.AddPass(rg, this);
		god_rays_pass.AddPass(rg, this);
		if (clouds_pass.IsEnabled(this)) clouds_pass.AddPass(rg, this);
		if(reflections_pass.IsEnabled(this)) reflections_pass.AddPass(rg, this);
		if(film_effects_pass.IsEnabled(this)) film_effects_pass.AddPass(rg, this);
		if (fog_pass.IsEnabled(this)) fog_pass.AddPass(rg, this);
		depth_of_field_pass.AddPass(rg, this);
		if(upscaler_pass.IsEnabled(this)) upscaler_pass.AddPass(rg, this);
		
		//for (uint32 i = 0; i < PostEffectType_Count; ++i)
		//{
		//	if (post_effects[i]->IsEnabled(this)) post_effects[i]->AddPass(rg, this);
		//}
		if (taa_pass.IsEnabled(this)) taa_pass.AddPass(rg, this);
		
		if (motion_blur) final_resource = motion_blur_pass.AddPass(rg, final_resource);
		if (automatic_exposure) automatic_exposure_pass.AddPasses(rg, final_resource);
		if (bloom) bloom_pass.AddPass(rg, final_resource);
		
		if (cas && !upscaler_pass.IsEnabled(this) && HasTAA())
		{
			final_resource = cas_pass.AddPass(rg, final_resource);
		}
		
		if (fxaa)
		{
			tonemap_pass.AddPass(rg, final_resource, RG_NAME(TonemapOutput));
			fxaa_pass.AddPass(rg, RG_NAME(TonemapOutput));
		}
		else
		{
			AddTonemapPass(rg, final_resource);
		}
	}

	void PostProcessor::AddTonemapPass(RenderGraph& rg, RGResourceName input)
	{
		tonemap_pass.AddPass(rg, input);
	}

	void PostProcessor::OnResize(uint32 w, uint32 h)
	{
		display_width = w, display_height = h;
		upscaler_pass.OnResize(w, h);
		taa_pass.OnResize(w, h);
		motion_blur_pass.OnResize(w, h);
		bloom_pass.OnResize(w, h);
		automatic_exposure_pass.OnResize(w, h);
		fxaa_pass.OnResize(w, h);
		tonemap_pass.OnResize(w, h);
	}

	void PostProcessor::OnRenderResolutionChanged(uint32 w, uint32 h)
	{
		render_width = w, render_height = h;

		ssao_pass.OnResize(w, h);
		hbao_pass.OnResize(w, h);
		cacao_pass.OnResize(w, h);
		rtao_pass.OnResize(w, h);

		clouds_pass.OnResize(w, h);
		lens_flare_pass.OnResize(w, h);
		fog_pass.OnResize(w, h);
		velocity_buffer_pass.OnResize(w, h);
		god_rays_pass.OnResize(w, h);
		film_effects_pass.OnResize(w, h);

		depth_of_field_pass.OnResize(w, h);
		sun_pass.OnResize(w, h);
	}

	void PostProcessor::OnSceneInitialized()
	{
		ssao_pass.OnSceneInitialized();
		hbao_pass.OnSceneInitialized();
		automatic_exposure_pass.OnSceneInitialized();
		clouds_pass.OnSceneInitialized();
		depth_of_field_pass.OnSceneInitialized();
		lens_flare_pass.OnSceneInitialized();
		tonemap_pass.OnSceneInitialized(); 
	}

	RGResourceName PostProcessor::GetFinalResource() const
	{
		return final_resource;
	}

	bool PostProcessor::HasTAA() const
	{
		return taa_pass.IsEnabled(this);
	}

	void PostProcessor::InitializePostEffects()
	{
		post_effects[PostEffectType_MotionVectors]	= std::make_unique<MotionVectorsPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_LensFlare]		= std::make_unique<LensFlarePass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Sun]			= std::make_unique<SunPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_GodRays]		= std::make_unique<GodRaysPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Clouds]			= std::make_unique<VolumetricCloudsPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Reflections]	= std::make_unique<ReflectionPassGroup>(gfx, render_width, render_height);
		post_effects[PostEffectType_FilmEffects]	= std::make_unique<FilmEffectsPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Fog]			= std::make_unique<ExponentialHeightFogPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_DepthOfField]	= std::make_unique<DepthOfFieldPassGroup>(gfx, render_width, render_height);
		post_effects[PostEffectType_Upscaler]		= std::make_unique<UpscalerPassGroup>(gfx, render_width, render_height);
		post_effects[PostEffectType_TAA]			= std::make_unique<TAAPass>(gfx, render_width, render_height);
	}

	RGResourceName PostProcessor::AddHDRCopyPass(RenderGraph& rg)
	{
		struct CopyPassData
		{
			RGTextureCopySrcId copy_src;
			RGTextureCopyDstId copy_dst;
		};

		rg.AddPass<CopyPassData>("Copy HDR Pass",
			[=](CopyPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc postprocess_desc{};
				postprocess_desc.width = render_width;
				postprocess_desc.height = render_height;
				postprocess_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_NAME(PostprocessMain), postprocess_desc);
				data.copy_dst = builder.WriteCopyDstTexture(RG_NAME(PostprocessMain));
				data.copy_src = builder.ReadCopySrcTexture(RG_NAME(HDR_RenderTarget));
			},
			[=](CopyPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxTexture const& src_texture = context.GetCopySrcTexture(data.copy_src);
				GfxTexture& dst_texture = context.GetCopyDstTexture(data.copy_dst);
				cmd_list->CopyTexture(dst_texture, src_texture);
			}, RGPassType::Copy, RGPassFlags::None);

		return RG_NAME(PostprocessMain);
	}

	void PostProcessor::PostprocessorGUI()
	{
		clouds_pass.GUI();
		reflections_pass.GUI();
		film_effects_pass.GUI();
		fog_pass.GUI();
		depth_of_field_pass.GUI();
		upscaler_pass.GUI();
		taa_pass.GUI();
		GUI_Command([&]()
			{
				static int current_ao_type = (int)ambient_occlusion;
				if (ImGui::TreeNode("Post-processing"))
				{
					if (ImGui::Combo("Ambient Occlusion", &current_ao_type, "None\0SSAO\0HBAO\0CACAO\0RTAO\0", 5))
					{
						if (!ray_tracing_supported && current_ao_type == 4) current_ao_type = 0;
						ambient_occlusion = static_cast<AmbientOcclusionType>(current_ao_type);
						cvar_ambient_occlusion->Set(current_ao_type);
					}
					
					if (ImGui::Checkbox("Automatic Exposure", &automatic_exposure)) cvar_autoexposure->Set(automatic_exposure);
					if (ImGui::Checkbox("Bloom", &bloom)) cvar_bloom->Set(bloom);
					if (ImGui::Checkbox("Motion Blur", &motion_blur)) cvar_motion_blur->Set(motion_blur);
					if (ImGui::Checkbox("FXAA", &fxaa)) cvar_fxaa->Set(fxaa);
					if (HasTAA())
					{
						if (ImGui::Checkbox("CAS", &cas)) cvar_cas->Set(cas);
					}

					ImGui::TreePop();
				}

			}, GUICommandGroup_None);
	}

	bool PostProcessor::HasUpscaler() const
	{
		return upscaler_pass.IsEnabled(this);
	}

}

