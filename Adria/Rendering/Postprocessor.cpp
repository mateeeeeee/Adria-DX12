#include "PostProcessor.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "RainPass.h"
#include "SunPass.h"
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
#include "TAAPass.h"
#include "UpscalerPassGroup.h"
#include "FFXCASPass.h"
#include "FXAAPass.h"
#include "ToneMapPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "Logging/Logger.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{
	static TAutoConsoleVariable<int>  AmbientOcclusion("r.AmbientOcclusion", 1, "0 - No AO, 1 - SSAO, 2 - HBAO, 3 - CACAO, 4 - RTAO");
	
	enum AmbientOcclusionType : uint8
	{
		AmbientOcclusionType_None,
		AmbientOcclusionType_SSAO,
		AmbientOcclusionType_HBAO,
		AmbientOcclusionType_CACAO,
		AmbientOcclusionType_RTAO
	};

	PostProcessor::PostProcessor(GfxDevice* gfx, entt::registry& reg, uint32 width, uint32 height)
		: gfx(gfx), reg(reg), display_width(width), display_height(height), render_width(width), render_height(height),
		ssao_pass(gfx, width, height), hbao_pass(gfx, width, height), rtao_pass(gfx, width, height), cacao_pass(gfx, width, height)
	{
		InitializePostEffects();
		ray_tracing_supported = gfx->GetCapabilities().SupportsRayTracing();
	}

	PostProcessor::~PostProcessor() = default;

	void PostProcessor::OnRainEvent(bool enabled)
	{
		GetPostEffect<VolumetricCloudsPass>()->OnRainEvent(enabled);
	}

	void PostProcessor::AddAmbientOcclusionPass(RenderGraph& rg)
	{
		switch (AmbientOcclusion.Get())
		{
		case AmbientOcclusionType_SSAO:  ssao_pass.AddPass(rg); break;
		case AmbientOcclusionType_HBAO:  hbao_pass.AddPass(rg); break;
		case AmbientOcclusionType_CACAO: cacao_pass.AddPass(rg); break;
		case AmbientOcclusionType_RTAO:  rtao_pass.AddPass(rg); break;
		}
	}

	void PostProcessor::AddPasses(RenderGraph& rg)
	{
		final_resource = AddHDRCopyPass(rg);

		for (uint32 i = 0; i < PostEffectType_Count; ++i)
		{
			if (post_effects[i]->IsEnabled(this)) post_effects[i]->AddPass(rg, this);
		}
	}

	void PostProcessor::AddTonemapPass(RenderGraph& rg, RGResourceName input)
	{
		final_resource = input;
		is_path_tracing_path = true;
		post_effects[PostEffectType_ToneMap]->AddPass(rg, this);
		is_path_tracing_path = false;
	}

	void PostProcessor::AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate delegate)
	{
		GetPostEffect<UpscalerPassGroup>()->AddRenderResolutionChangedCallback(delegate);
	}

	void PostProcessor::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::Combo("Ambient Occlusion Type", AmbientOcclusion.GetPtr(), "None\0SSAO\0HBAO\0CACAO\0RTAO\0", 5))
				{
					if (!ray_tracing_supported && AmbientOcclusion.Get() == 4) AmbientOcclusion->Set(AmbientOcclusionType_SSAO); 
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_AO);
		switch (AmbientOcclusion.Get())
		{
		case AmbientOcclusionType_SSAO:  ssao_pass.GUI();  break;
		case AmbientOcclusionType_HBAO:  hbao_pass.GUI();  break;
		case AmbientOcclusionType_CACAO: cacao_pass.GUI(); break;
		case AmbientOcclusionType_RTAO:  rtao_pass.GUI();  break;
		}

		for (auto& post_effect : post_effects)
		{
			if (post_effect->IsGUIVisible(this)) post_effect->GUI();
		}
	}

	void PostProcessor::OnResize(uint32 w, uint32 h)
	{
		display_width = w, display_height = h;
		for (uint32 i = PostEffectType_Upscaler; i < PostEffectType_Count; ++i)
		{
			post_effects[i]->OnResize(w, h);
		}
	}

	void PostProcessor::OnRenderResolutionChanged(uint32 w, uint32 h)
	{
		render_width = w, render_height = h;

		ssao_pass.OnResize(w, h);
		hbao_pass.OnResize(w, h);
		cacao_pass.OnResize(w, h);
		rtao_pass.OnResize(w, h);

		for (uint32 i = 0; i < PostEffectType_Upscaler; ++i)
		{
			post_effects[i]->OnResize(w, h);
		}
	}

	void PostProcessor::OnSceneInitialized()
	{
		ssao_pass.OnSceneInitialized();
		hbao_pass.OnSceneInitialized();
		for (auto& post_effect : post_effects)
		{
			post_effect->OnSceneInitialized();
		}
	}

	RGResourceName PostProcessor::GetFinalResource() const
	{
		return final_resource;
	}

	bool PostProcessor::HasTAA() const
	{
		return post_effects[PostEffectType_TAA]->IsEnabled(this);
	}

	bool PostProcessor::HasFXAA() const
	{
		return post_effects[PostEffectType_FXAA]->IsEnabled(this);
	}

	bool PostProcessor::IsPathTracing() const
	{
		return is_path_tracing_path;
	}

	bool PostProcessor::NeedsVelocityBuffer() const
	{
		return HasTAA() || HasUpscaler() || post_effects[PostEffectType_Clouds]->IsEnabled(this) || post_effects[PostEffectType_MotionBlur]->IsEnabled(this);
	}

	bool PostProcessor::HasUpscaler() const
	{
		return post_effects[PostEffectType_Upscaler]->IsEnabled(this);
	}

	void PostProcessor::InitializePostEffects()
	{
		post_effects[PostEffectType_MotionVectors]	= std::make_unique<MotionVectorsPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_LensFlare]		= std::make_unique<LensFlarePass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Sun]			= std::make_unique<SunPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_GodRays]		= std::make_unique<GodRaysPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Clouds]			= std::make_unique<VolumetricCloudsPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Reflection]		= std::make_unique<ReflectionPassGroup>(gfx, render_width, render_height);
		post_effects[PostEffectType_FilmEffects]	= std::make_unique<FilmEffectsPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Fog]			= std::make_unique<ExponentialHeightFogPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_DepthOfField]	= std::make_unique<DepthOfFieldPassGroup>(gfx, render_width, render_height);
		post_effects[PostEffectType_Upscaler]		= std::make_unique<UpscalerPassGroup>(gfx, render_width, render_height);
		post_effects[PostEffectType_TAA]			= std::make_unique<TAAPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_MotionBlur]		= std::make_unique<MotionBlurPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_AutoExposure]	= std::make_unique<AutoExposurePass>(gfx, render_width, render_height);
		post_effects[PostEffectType_Bloom]			= std::make_unique<BloomPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_CAS]			= std::make_unique<FFXCASPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_ToneMap]		= std::make_unique<ToneMapPass>(gfx, render_width, render_height);
		post_effects[PostEffectType_FXAA]			= std::make_unique<FXAAPass>(gfx, render_width, render_height);

		GetPostEffect<UpscalerPassGroup>()->AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate::CreateMember(&PostProcessor::OnRenderResolutionChanged, *this));
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


	template<typename PostEffectT> requires std::is_base_of_v<PostEffect, PostEffectT>
	PostEffectT* PostProcessor::GetPostEffect() const
	{
		if constexpr (std::is_same_v<PostEffectT, MotionVectorsPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_MotionVectors].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, LensFlarePass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_LensFlare].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, SunPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_Sun].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, GodRaysPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_GodRays].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, VolumetricCloudsPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_Clouds].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, ReflectionPassGroup>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_Reflection].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, FilmEffectsPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_FilmEffects].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, ExponentialHeightFogPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_Fog].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, DepthOfFieldPassGroup>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_DepthOfField].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, UpscalerPassGroup>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_Upscaler].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, TAAPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_TAA].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, MotionBlurPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_MotionBlur].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, AutoExposurePass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_AutoExposure].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, BloomPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_Bloom].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, FFXCASPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_CAS].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, ToneMapPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_ToneMap].get());
		}
		else if constexpr (std::is_same_v<PostEffectT, FXAAPass>)
		{
			return static_cast<PostEffectT*>(post_effects[PostEffectType_FXAA].get());
		}
		else
		{
			return nullptr;
		}
	}
}

