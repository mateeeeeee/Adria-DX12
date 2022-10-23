#include "Postprocessor.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "../Logging/Logger.h"


using namespace DirectX;

namespace adria
{
	Postprocessor::Postprocessor(entt::registry& reg, TextureManager& texture_manager, uint32 width, uint32 height)
		: reg(reg), texture_manager(texture_manager), width(width), height(height),
		blur_pass(width, height), copy_to_texture_pass(width, height), generate_mips_pass(width, height),
		add_textures_pass(width, height), automatic_exposure_pass(width, height),
		lens_flare_pass(texture_manager, width, height),
		clouds_pass(texture_manager, width, height), ssr_pass(width, height), fog_pass(width, height),
		dof_pass(width, height), bloom_pass(width, height), velocity_buffer_pass(width, height),
		motion_blur_pass(width, height), taa_pass(width, height), god_rays_pass(width, height),
		bokeh_pass(texture_manager, width, height)
	{}

	void Postprocessor::AddPasses(RenderGraph& rg, PostprocessSettings const& _settings)
	{
		settings = _settings;
		auto lights = reg.view<Light>();
		if (settings.motion_blur || HasAnyFlag(settings.anti_aliasing, AntiAliasing_TAA))
		{
			velocity_buffer_pass.AddPass(rg);
		}
		final_resource = AddHDRCopyPass(rg);
		
		for (entt::entity light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (!light_data.active || !light_data.lens_flare) continue;
			lens_flare_pass.AddPass(rg, light_data);
		}

		if (settings.clouds)
		{
			clouds_pass.AddPass(rg);
			blur_pass.AddPass(rg, RG_RES_NAME(CloudsOutput), RG_RES_NAME(BlurredCloudsOutput), "Volumetric Clouds");
			copy_to_texture_pass.AddPass(rg, RG_RES_NAME(PostprocessMain), RG_RES_NAME(BlurredCloudsOutput), EBlendMode::AlphaBlend);
		}
		if (settings.automatic_exposure) automatic_exposure_pass.AddPasses(rg, final_resource);

		if (settings.reflections == EReflections::SSR) final_resource = ssr_pass.AddPass(rg, final_resource);
		else if (settings.reflections == EReflections::RTR)
		{
			copy_to_texture_pass.AddPass(rg, final_resource, RG_RES_NAME(RTR_Output), EBlendMode::AdditiveBlend);
		}

		if (settings.fog) final_resource = fog_pass.AddPass(rg, final_resource);
		if (settings.dof)
		{
			blur_pass.AddPass(rg, final_resource, RG_RES_NAME(BlurredDofInput), "DoF");
			if (settings.bokeh) bokeh_pass.AddPass(rg, final_resource);
			final_resource = dof_pass.AddPass(rg, final_resource);
		}
		if (settings.motion_blur) final_resource = motion_blur_pass.AddPass(rg, final_resource);
		if (settings.bloom) final_resource = bloom_pass.AddPass(rg, final_resource);

		for (entt::entity light_entity : lights)
		{
			auto const& light = lights.get<Light>(light_entity);
			if (!light.active) continue;

			if (light.type == ELightType::Directional)
			{
				AddSunPass(rg, light_entity);
				if (light.god_rays)
				{
					god_rays_pass.AddPass(rg, light);
					copy_to_texture_pass.AddPass(rg, final_resource, RG_RES_NAME(GodRaysOutput), EBlendMode::AdditiveBlend);
				}
				else
				{
					copy_to_texture_pass.AddPass(rg, final_resource, RG_RES_NAME(SunOutput), EBlendMode::AdditiveBlend);
				}
				break;
			}
		}
		if (HasAnyFlag(settings.anti_aliasing, AntiAliasing_TAA))
		{
			rg.ImportTexture(RG_RES_NAME(HistoryBuffer), history_buffer.get());
			final_resource = taa_pass.AddPass(rg, final_resource, RG_RES_NAME(HistoryBuffer));
			AddHistoryCopyPass(rg);
		}
	}

	void Postprocessor::OnResize(GraphicsDevice* gfx, uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(w, h);
		add_textures_pass.OnResize(w, h);
		copy_to_texture_pass.OnResize(w, h);
		generate_mips_pass.OnResize(w, h);
		automatic_exposure_pass.OnResize(w, h);
		lens_flare_pass.OnResize(w, h);
		clouds_pass.OnResize(w, h);
		ssr_pass.OnResize(w, h);
		fog_pass.OnResize(w, h);
		dof_pass.OnResize(w, h);
		bloom_pass.OnResize(w, h);
		velocity_buffer_pass.OnResize(w, h);
		motion_blur_pass.OnResize(w, h);
		taa_pass.OnResize(w, h);
		god_rays_pass.OnResize(w, h);
		bokeh_pass.OnResize(w, h);

		TextureDesc render_target_desc{};
		render_target_desc.format = EFormat::R16G16B16A16_FLOAT;
		render_target_desc.width = width;
		render_target_desc.height = height;
		render_target_desc.bind_flags = EBindFlag::ShaderResource;
		render_target_desc.initial_state = EResourceState::CopyDest;
		history_buffer = std::make_unique<Texture>(gfx, render_target_desc);
	}
	void Postprocessor::OnSceneInitialized(GraphicsDevice* gfx)
	{
		automatic_exposure_pass.OnSceneInitialized(gfx);
		lens_flare_pass.OnSceneInitialized(gfx);
		clouds_pass.OnSceneInitialized(gfx);
		bokeh_pass.OnSceneInitialized(gfx);

		TextureDesc render_target_desc{};
		render_target_desc.format = EFormat::R16G16B16A16_FLOAT;
		render_target_desc.width = width;
		render_target_desc.height = height;
		render_target_desc.bind_flags = EBindFlag::ShaderResource;
		render_target_desc.initial_state = EResourceState::CopyDest;
		history_buffer = std::make_unique<Texture>(gfx, render_target_desc);
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
				postprocess_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(PostprocessMain), postprocess_desc);
				data.copy_dst = builder.WriteCopyDstTexture(RG_RES_NAME(PostprocessMain));
				data.copy_src = builder.ReadCopySrcTexture(RG_RES_NAME(HDR_RenderTarget));
			},
			[=](CopyPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Texture const& src_texture = context.GetCopySrcTexture(data.copy_src);
				Texture const& dst_texture = context.GetCopyDstTexture(data.copy_dst);
				cmd_list->CopyResource(dst_texture.GetNative(), src_texture.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::None);

		return RG_RES_NAME(PostprocessMain);
	}
	void Postprocessor::AddHistoryCopyPass(RenderGraph& rg)
	{
		struct CopyPassData
		{
			RGTextureCopySrcId copy_src;
			RGTextureCopyDstId copy_dst;
		};
		RGResourceName last_resource = final_resource;

		rg.AddPass<CopyPassData>("History Copy Pass",
			[=](CopyPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc history_desc{};
				history_desc.width = width;
				history_desc.height = height;
				history_desc.format = EFormat::R16G16B16A16_FLOAT;

				data.copy_dst = builder.WriteCopyDstTexture(RG_RES_NAME(HistoryBuffer));
				data.copy_src = builder.ReadCopySrcTexture(last_resource);
			},
			[=](CopyPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Texture const& src_texture = context.GetCopySrcTexture(data.copy_src);
				Texture const& dst_texture = context.GetCopyDstTexture(data.copy_dst);
				cmd_list->CopyResource(dst_texture.GetNative(), src_texture.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::None);
	}
	void Postprocessor::AddSunPass(RenderGraph& rg, entt::entity sun)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = final_resource;

		rg.AddPass<void>("Sun Pass",
			[=](RenderGraphBuilder& builder)
			{
				RGTextureDesc sun_output_desc{};
				sun_output_desc.format = EFormat::R16G16B16A16_FLOAT;
				sun_output_desc.width = width;
				sun_output_desc.height = height;
				sun_output_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_RES_NAME(SunOutput), sun_output_desc);
				builder.ReadDepthStencil(RG_RES_NAME(DepthStencil), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(SunOutput), ERGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Forward));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Sun));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				
				auto [transform, mesh, material] = reg.get<Transform, Mesh, Material>(sun);

				ObjectCBuffer object_cbuf_data{};
				object_cbuf_data.model = transform.current_transform;
				object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, object_cbuf_data.model));

				DynamicAllocation object_allocation = dynamic_allocator->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				object_allocation.Update(object_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

				MaterialCBuffer material_cbuf_data{};
				material_cbuf_data.diffuse = material.diffuse;
				DynamicAllocation material_allocation = dynamic_allocator->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				material_allocation.Update(material_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

				D3D12_CPU_DESCRIPTOR_HANDLE diffuse_handle = texture_manager.GetSRV(material.albedo_texture);
				uint32 src_range_size = 1;

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);

				device->CopyDescriptorsSimple(1, dst_descriptor, diffuse_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));
				mesh.Draw(cmd_list);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}
}

