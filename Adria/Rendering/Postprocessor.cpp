#include "Postprocessor.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"

#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{
	Postprocessor::Postprocessor(entt::registry& reg, uint32 width, uint32 height)
		: reg(reg), width(width), height(height),
		blur_pass(width, height), copy_to_texture_pass(width, height),
		add_textures_pass(width, height), automatic_exposure_pass(width, height),
		lens_flare_pass(width, height),
		clouds_pass(width, height), ssr_pass(width, height), fog_pass(width, height),
		dof_pass(width, height), bloom_pass(width, height), velocity_buffer_pass(width, height),
		motion_blur_pass(width, height), taa_pass(width, height), god_rays_pass(width, height),
		bokeh_pass(width, height)
	{}

	void Postprocessor::AddPasses(RenderGraph& rg, PostprocessSettings const& _settings)
	{
		settings = _settings;
		auto lights = reg.view<Light>();
		if (settings.motion_blur || HasAnyFlag(settings.anti_aliasing, AntiAliasing_TAA) || settings.clouds)
		{
			velocity_buffer_pass.AddPass(rg);
		}
		final_resource = AddHDRCopyPass(rg);

		for (entt::entity light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (!light_data.active || !light_data.lens_flare) continue;
			lens_flare_pass.AddPass2(rg, light_data);
		}

		if (settings.clouds) clouds_pass.AddPass(rg);

		if (settings.reflections == Reflections::SSR) final_resource = ssr_pass.AddPass(rg, final_resource);
		else if (settings.reflections == Reflections::RTR)
		{
			copy_to_texture_pass.AddPass(rg, final_resource, RG_RES_NAME(RTR_Output), BlendMode::AdditiveBlend);
		}

		if (settings.fog) final_resource = fog_pass.AddPass(rg, final_resource);
		if (settings.dof)
		{
			blur_pass.AddPass(rg, final_resource, RG_RES_NAME(BlurredDofInput), " DoF ");
			if (settings.bokeh) bokeh_pass.AddPass(rg, final_resource);
			final_resource = dof_pass.AddPass(rg, final_resource);
		}
		if (settings.motion_blur) final_resource = motion_blur_pass.AddPass(rg, final_resource);

		for (entt::entity light_entity : lights)
		{
			auto const& light = lights.get<Light>(light_entity);
			if (!light.active) continue;

			if (light.type == LightType::Directional)
			{
				AddSunPass(rg, light_entity);
				if (light.god_rays)
				{
					god_rays_pass.AddPass(rg, light);
					copy_to_texture_pass.AddPass(rg, final_resource, RG_RES_NAME(GodRaysOutput), BlendMode::AdditiveBlend);
				}
				else
				{
					copy_to_texture_pass.AddPass(rg, final_resource, RG_RES_NAME(SunOutput), BlendMode::AdditiveBlend);
				}
				break;
			}
		}
		if (settings.automatic_exposure) automatic_exposure_pass.AddPasses(rg, final_resource);
		if (settings.bloom) bloom_pass.AddPass(rg, final_resource);

		if (HasAnyFlag(settings.anti_aliasing, AntiAliasing_TAA))
		{
			rg.ImportTexture(RG_RES_NAME(HistoryBuffer), history_buffer.get());
			final_resource = taa_pass.AddPass(rg, final_resource, RG_RES_NAME(HistoryBuffer));
			rg.ExportTexture(final_resource, history_buffer.get());
		}

	}

	void Postprocessor::OnResize(GfxDevice* gfx, uint32 w, uint32 h)
	{
		width = w, height = h;
		clouds_pass.OnResize(gfx, w, h);
		blur_pass.OnResize(w, h);
		add_textures_pass.OnResize(w, h);
		copy_to_texture_pass.OnResize(w, h);
		automatic_exposure_pass.OnResize(w, h);
		lens_flare_pass.OnResize(w, h);
		ssr_pass.OnResize(w, h);
		fog_pass.OnResize(w, h);
		dof_pass.OnResize(w, h);
		bloom_pass.OnResize(w, h);
		velocity_buffer_pass.OnResize(w, h);
		motion_blur_pass.OnResize(w, h);
		taa_pass.OnResize(w, h);
		god_rays_pass.OnResize(w, h);
		bokeh_pass.OnResize(w, h);

		if (history_buffer)
		{
			GfxTextureDesc render_target_desc = history_buffer->GetDesc();
			render_target_desc.width = width;
			render_target_desc.height = height;
			history_buffer = gfx->CreateTexture(render_target_desc);
		}
	}
	void Postprocessor::OnSceneInitialized(GfxDevice* gfx)
	{
		automatic_exposure_pass.OnSceneInitialized(gfx);
		lens_flare_pass.OnSceneInitialized(gfx);
		clouds_pass.OnSceneInitialized(gfx);
		bokeh_pass.OnSceneInitialized(gfx);

		GfxTextureDesc render_target_desc{};
		render_target_desc.format = GfxFormat::R16G16B16A16_FLOAT;
		render_target_desc.width = width;
		render_target_desc.height = height;
		render_target_desc.bind_flags = GfxBindFlag::ShaderResource;
		render_target_desc.initial_state = GfxResourceState::CopyDest;
		history_buffer = gfx->CreateTexture(render_target_desc);
	}
	RGResourceName Postprocessor::GetFinalResource() const
	{
		return final_resource;
	}

	RGResourceName Postprocessor::AddHDRCopyPass(RenderGraph& rg)
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
				postprocess_desc.width = width;
				postprocess_desc.height = height;
				postprocess_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(PostprocessMain), postprocess_desc);
				data.copy_dst = builder.WriteCopyDstTexture(RG_RES_NAME(PostprocessMain));
				data.copy_src = builder.ReadCopySrcTexture(RG_RES_NAME(HDR_RenderTarget));
			},
			[=](CopyPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxTexture const& src_texture = context.GetCopySrcTexture(data.copy_src);
				GfxTexture& dst_texture = context.GetCopyDstTexture(data.copy_dst);
				cmd_list->CopyTexture(dst_texture, src_texture);
			}, RGPassType::Copy, RGPassFlags::None);

		return RG_RES_NAME(PostprocessMain);
	}
	void Postprocessor::AddSunPass(RenderGraph& rg, entt::entity sun)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		RGResourceName last_resource = final_resource;

		rg.AddPass<void>("Sun Pass",
			[=](RenderGraphBuilder& builder)
			{
				RGTextureDesc sun_output_desc{};
				sun_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				sun_output_desc.width = width;
				sun_output_desc.height = height;
				sun_output_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_RES_NAME(SunOutput), sun_output_desc);
				builder.ReadDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(SunOutput), RGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Sun));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				auto [transform, mesh, material] = reg.get<Transform, SubMesh, Material>(sun);

				struct Constants
				{
					Matrix model_matrix;
					Vector3 diffuse_color;
					uint32   diffuse_idx;
				} constants =
				{
					.model_matrix = transform.current_transform,
					.diffuse_color = Vector3(material.base_color),
					.diffuse_idx = (uint32)material.albedo_texture
				};
				GfxDynamicAllocation allocation = dynamic_allocator->AllocateCBuffer<Constants>();
				allocation.Update(constants);
				cmd_list->SetRootCBV(2, allocation.gpu_address);
				Draw(mesh, cmd_list);
			}, RGPassType::Graphics, RGPassFlags::None);
	}
}

