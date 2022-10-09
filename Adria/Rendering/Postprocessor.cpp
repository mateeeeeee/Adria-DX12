#include "Postprocessor.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
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
		motion_blur_pass(width, height), taa_pass(width, height), god_rays_pass(width, height)
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

		if (settings.automatic_exposure)
		{
			automatic_exposure_pass.AddPasses(rg, final_resource);
		}

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

		if (settings.reflections == EReflections::SSR)
		{
			final_resource = ssr_pass.AddPass(rg, final_resource);
		}
		else if (settings.reflections == EReflections::RTR)
		{
			copy_to_texture_pass.AddPass(rg, final_resource, RG_RES_NAME(RTR_Output), EBlendMode::AdditiveBlend);
		}

		if (settings.fog)
		{
			final_resource = fog_pass.AddPass(rg, final_resource);
		}
		if (settings.dof)
		{
			blur_pass.AddPass(rg, final_resource, RG_RES_NAME(BlurredDofInput), "DoF");
			if (settings.bokeh)
			{
				AddGenerateBokehPass(rg);
				AddDrawBokehPass(rg);
			}
			final_resource = dof_pass.AddPass(rg, final_resource);
		}
		if (settings.motion_blur)
		{
			final_resource = motion_blur_pass.AddPass(rg, final_resource);
		}
		if (settings.bloom)
		{
			final_resource = bloom_pass.AddPass(rg, final_resource);
		}
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

		hex_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Hex.dds");
		oct_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Oct.dds");
		circle_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Circle.dds");
		cross_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Cross.dds");

		TextureDesc render_target_desc{};
		render_target_desc.format = EFormat::R16G16B16A16_FLOAT;
		render_target_desc.width = width;
		render_target_desc.height = height;
		render_target_desc.bind_flags = EBindFlag::ShaderResource;
		render_target_desc.initial_state = EResourceState::CopyDest;
		history_buffer = std::make_unique<Texture>(gfx, render_target_desc);

		BufferDesc reset_buffer_desc{};
		reset_buffer_desc.size = sizeof(uint32);
		reset_buffer_desc.resource_usage = EResourceUsage::Upload;
		uint32 initial_data[] = { 0 };
		counter_reset_buffer = std::make_unique<Buffer>(gfx, reset_buffer_desc, initial_data);

		BufferDesc buffer_desc{};
		buffer_desc.size = 4 * sizeof(uint32);
		buffer_desc.bind_flags = EBindFlag::None;
		buffer_desc.misc_flags = EBufferMiscFlag::IndirectArgs;
		buffer_desc.resource_usage = EResourceUsage::Default;

		uint32 init_data[] = { 0,1,0,0 };
		bokeh_indirect_buffer = std::make_unique<Buffer>(gfx, buffer_desc, init_data);

		D3D12_INDIRECT_ARGUMENT_DESC args[1];
		args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
		D3D12_COMMAND_SIGNATURE_DESC command_signature_desc{};
		command_signature_desc.NumArgumentDescs = 1;
		command_signature_desc.pArgumentDescs = args;
		command_signature_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
		BREAK_IF_FAILED(gfx->GetDevice()->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&bokeh_command_signature)));
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

				D3D12_CPU_DESCRIPTOR_HANDLE diffuse_handle = texture_manager.CpuDescriptorHandle(material.albedo_texture);
				uint32 src_range_size = 1;

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);

				device->CopyDescriptorsSimple(1, dst_descriptor, diffuse_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));
				mesh.Draw(cmd_list);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void Postprocessor::AddGenerateBokehPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = final_resource;
		struct BokehCounterResetPassData
		{
			RGBufferCopySrcId src;
			RGBufferCopyDstId dst;
		};
		
		rg.AddPass<BokehCounterResetPassData>("Bokeh Counter Reset Pass",
			[=](BokehCounterResetPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc bokeh_counter_desc{};
				bokeh_counter_desc.size = sizeof(uint32);
				builder.DeclareBuffer(RG_RES_NAME(BokehCounter), bokeh_counter_desc);
				data.dst = builder.WriteCopyDstBuffer(RG_RES_NAME(BokehCounter));
			},
			[=](BokehCounterResetPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Buffer const& bokeh_counter = context.GetCopyDstBuffer(data.dst);
				cmd_list->CopyBufferRegion(bokeh_counter.GetNative(), 0, counter_reset_buffer->GetNative(), 0, sizeof(uint32));
			}, ERGPassType::Copy, ERGPassFlags::None);
			
		struct BokehGeneratePassData
		{
			RGTextureReadOnlyId input_srv;
			RGTextureReadOnlyId depth_srv;
			RGBufferReadWriteId bokeh_uav;
		};

		rg.AddPass<BokehGeneratePassData>("Bokeh Generate Pass",
			[=](BokehGeneratePassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc bokeh_desc{};
				bokeh_desc.resource_usage = EResourceUsage::Default;
				bokeh_desc.misc_flags = EBufferMiscFlag::BufferStructured;
				bokeh_desc.stride = sizeof(Bokeh);
				bokeh_desc.size = bokeh_desc.stride * width * height;
				builder.DeclareBuffer(RG_RES_NAME(Bokeh), bokeh_desc);
				data.bokeh_uav = builder.WriteBuffer(RG_RES_NAME(Bokeh), RG_RES_NAME(BokehCounter));
				data.input_srv = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](BokehGeneratePassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		
				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::BokehGenerate));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::BokehGenerate));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(1, global_data.postprocess_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(2, global_data.compute_cbuffer_address);
		
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.input_srv), context.GetReadOnlyTexture(data.depth_srv) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1 };
				uint32 dst_range_sizes[] = { 2 };
				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));
		
				D3D12_CPU_DESCRIPTOR_HANDLE bokeh_uav = context.GetReadWriteBuffer(data.bokeh_uav);
				descriptor_index = descriptor_allocator->Allocate();
		
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), bokeh_uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
				cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->Dispatch((uint32)std::ceil(width / 32.0f), (uint32)std::ceil(height / 32.0f), 1);

			}, ERGPassType::Compute, ERGPassFlags::None);
		
		struct BokehCopyToIndirectBufferPass
		{
			RGBufferCopySrcId src;
			RGBufferCopyDstId dst;
		};
		rg.ImportBuffer(RG_RES_NAME(BokehIndirectDraw), bokeh_indirect_buffer.get());
		rg.AddPass<BokehCopyToIndirectBufferPass>("Bokeh Indirect Buffer Pass",
			[=](BokehCopyToIndirectBufferPass& data, RenderGraphBuilder& builder)
			{
				data.dst = builder.WriteCopyDstBuffer(RG_RES_NAME(BokehIndirectDraw));
				data.src = builder.ReadCopySrcBuffer(RG_RES_NAME(BokehCounter));
			},
			[=](BokehCopyToIndirectBufferPass const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Buffer const& src_buffer = context.GetCopySrcBuffer(data.src);
				Buffer const& dst_buffer = context.GetCopyDstBuffer(data.dst);
				cmd_list->CopyBufferRegion(dst_buffer.GetNative(), 0, src_buffer.GetNative(), 0, src_buffer.GetDesc().size);
			}, ERGPassType::Copy, ERGPassFlags::None);
	}
	void Postprocessor::AddDrawBokehPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = final_resource;
		struct BokehDrawPassData
		{
			RGBufferReadOnlyId bokeh_srv;
			RGBufferIndirectArgsId bokeh_indirect_args;
		};

		rg.AddPass<BokehDrawPassData>("Bokeh Draw Pass",
			[=](BokehDrawPassData& data, RenderGraphBuilder& builder) 
			{
				builder.WriteRenderTarget(last_resource, ERGLoadStoreAccessOp::Preserve_Preserve);
				data.bokeh_srv = builder.ReadBuffer(RG_RES_NAME(Bokeh), ReadAccess_PixelShader);
				data.bokeh_indirect_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(BokehIndirectDraw));
				builder.SetViewport(width, height);
			},
			[=](BokehDrawPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				D3D12_CPU_DESCRIPTOR_HANDLE bokeh_descriptor{};
				switch (settings.bokeh_type)
				{
				case EBokehType::Hex:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(hex_bokeh_handle);
					break;
				case EBokehType::Oct:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(oct_bokeh_handle);
					break;
				case EBokehType::Circle:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(circle_bokeh_handle);
					break;
				case EBokehType::Cross:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(cross_bokeh_handle);
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Bokeh Type");
				}

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Bokeh));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Bokeh));

				OffsetType i = descriptor_allocator->AllocateRange(2);

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i),
					context.GetReadOnlyBuffer(data.bokeh_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1),
					bokeh_descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(i));
				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(i + 1));

				cmd_list->IASetVertexBuffers(0, 0, nullptr);
				cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

				Buffer const& indirect_args_buffer = context.GetIndirectArgsBuffer(data.bokeh_indirect_args);
				cmd_list->ExecuteIndirect(bokeh_command_signature.Get(), 1, indirect_args_buffer.GetNative(), 0,
					nullptr, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);
		
	}
}

