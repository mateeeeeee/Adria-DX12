#include "GBufferPass.h"
#include "../ConstantBuffers.h"
#include "../Components.h"
#include "../RendererGlobalData.h"
#include "../RootSigPSOManager.h"
#include "../../RenderGraph/RenderGraph.h"
#include "../../Graphics/GPUProfiler.h"
#include "../../tecs/registry.h"
#include "pix3.h"

namespace adria
{

	GBufferPass::GBufferPass(tecs::registry& reg, GPUProfiler& gpu_profiler, uint32 w, uint32 h) :
		reg{ reg }, gpu_profiler{ gpu_profiler }, width{ w }, height{ h }
	{
	}

	GBufferPassData const& GBufferPass::AddPass(RenderGraph& rendergraph, RGBlackboard& blackboard, bool profile_pass)
	{
		RendererGlobalData const& global_data = blackboard.GetChecked<RendererGlobalData>();

		return rendergraph.AddPass<GBufferPassData>("GBuffer Pass",
			[&](GBufferPassData& data, RenderGraphBuilder& builder)
			{
				D3D12_CLEAR_VALUE rtv_clear_value{};
				rtv_clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				rtv_clear_value.Color[0] = 0.0f;
				rtv_clear_value.Color[1] = 0.0f;
				rtv_clear_value.Color[2] = 0.0f;
				rtv_clear_value.Color[3] = 0.0f;

				TextureDesc gbuffer_desc{};
				gbuffer_desc.width = width;
				gbuffer_desc.height = height;
				gbuffer_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
				gbuffer_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				gbuffer_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
				gbuffer_desc.clear = rtv_clear_value;

				RGTextureRef gbuffer_normal = builder.CreateTexture("GBuffer Normal", gbuffer_desc);
				RGTextureRef gbuffer_albedo = builder.CreateTexture("GBuffer Albedo", gbuffer_desc);
				RGTextureRef gbuffer_emissive = builder.CreateTexture("GBuffer Emissive", gbuffer_desc);

				RGTextureRTVRef gbuffer_normal_rtv = builder.CreateRTV(gbuffer_normal);
				RGTextureRTVRef gbuffer_albedo_rtv = builder.CreateRTV(gbuffer_albedo);
				RGTextureRTVRef gbuffer_emissive_rtv = builder.CreateRTV(gbuffer_emissive);

				D3D12_CLEAR_VALUE clear_value{};
				clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				clear_value.DepthStencil.Depth = 1.0f;
				clear_value.DepthStencil.Stencil = 0;

				TextureDesc depth_desc{};
				depth_desc.width = width;
				depth_desc.height = height;
				depth_desc.format = DXGI_FORMAT_R32_TYPELESS;
				depth_desc.bind_flags = EBindFlag::DepthStencil | EBindFlag::ShaderResource;
				depth_desc.initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				depth_desc.clear = clear_value;
				RGTextureRef depth_stencil = builder.CreateTexture("Depth Stencil", depth_desc);
				RGTextureDSVRef depth_stencil_dsv = builder.CreateDSV(depth_stencil);

				builder.SetViewport(width, height);
				data.gbuffer_normal = builder.RenderTarget(gbuffer_normal_rtv, ERGLoadStoreAccessOp::Clear_Preserve);
				data.gbuffer_albedo = builder.RenderTarget(gbuffer_albedo_rtv, ERGLoadStoreAccessOp::Clear_Preserve);
				data.gbuffer_emissive = builder.RenderTarget(gbuffer_emissive_rtv, ERGLoadStoreAccessOp::Clear_Preserve);
				data.depth_stencil = builder.DepthStencil(depth_stencil_dsv, ERGLoadStoreAccessOp::Clear_Preserve);
			},
			[&](GBufferPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "GBuffer Pass");
				SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::GBufferPass, profile_pass);

				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();
				auto gbuffer_view = reg.view<Mesh, Transform, Material, Deferred, Visibility>();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::GbufferPBR));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::GbufferPBR));
				cmd_list->SetGraphicsRootConstantBufferView(0, (D3D12_GPU_VIRTUAL_ADDRESS)global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetFirstHandle());

				for (auto e : gbuffer_view)
				{
					auto [mesh, transform, material, visibility] = gbuffer_view.get<Mesh, Transform, Material, Visibility>(e);

					if (!visibility.camera_visible) continue;

					ObjectCBuffer object_cbuf_data{};
					object_cbuf_data.model = transform.current_transform;
					object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, object_cbuf_data.model));

					DynamicAllocation object_allocation = dynamic_allocator->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					object_allocation.Update(object_cbuf_data);
					cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

					MaterialCBuffer material_cbuf_data{};
					material_cbuf_data.albedo_factor = material.albedo_factor;
					material_cbuf_data.metallic_factor = material.metallic_factor;
					material_cbuf_data.roughness_factor = material.roughness_factor;
					material_cbuf_data.emissive_factor = material.emissive_factor;
					material_cbuf_data.albedo_idx = material.albedo_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.albedo_texture) : 0;
					material_cbuf_data.normal_idx = material.normal_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.normal_texture) : 0;
					material_cbuf_data.metallic_roughness_idx = material.metallic_roughness_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.metallic_roughness_texture) : 0;
					material_cbuf_data.emissive_idx = material.emissive_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.emissive_texture) : 0;

					DynamicAllocation material_allocation = dynamic_allocator->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					material_allocation.Update(material_cbuf_data);
					cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);
					mesh.Draw(cmd_list);
				}
			});
	}

	void GBufferPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

