#include <map>
#include "GBufferPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "../Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

namespace adria
{

	GBufferPass::GBufferPass(entt::registry& reg, GPUProfiler& gpu_profiler, uint32 w, uint32 h) :
		reg{ reg }, gpu_profiler{ gpu_profiler }, width{ w }, height{ h }
	{}

	void GBufferPass::AddPass(RenderGraph& rendergraph, bool profile_pass)
	{
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<void>("GBuffer Pass",
			[=](RenderGraphBuilder& builder)
			{
				RGTextureDesc gbuffer_desc{};
				gbuffer_desc.width = width;
				gbuffer_desc.height = height;
				gbuffer_desc.format = EFormat::R8G8B8A8_UNORM;
				gbuffer_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				
				builder.DeclareTexture(RG_RES_NAME(GBufferNormal), gbuffer_desc);
				builder.DeclareTexture(RG_RES_NAME(GBufferAlbedo), gbuffer_desc);
				builder.DeclareTexture(RG_RES_NAME(GBufferEmissive), gbuffer_desc);

				builder.WriteRenderTarget(RG_RES_NAME(GBufferNormal), ERGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), ERGLoadStoreAccessOp::Discard_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferEmissive), ERGLoadStoreAccessOp::Discard_Preserve);
				
				RGTextureDesc depth_desc{};
				depth_desc.width = width;
				depth_desc.height = height;
				depth_desc.format = EFormat::R32_TYPELESS;
				depth_desc.clear_value = ClearValue(1.0f, 0);
				builder.DeclareTexture(RG_RES_NAME(DepthStencil), depth_desc);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), ERGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();
				
				struct BatchParams
				{
					EPipelineState pso;
					auto operator<=>(BatchParams const&) const = default;
				};
				std::map<BatchParams, std::vector<entt::entity>> batched_entities;

				auto gbuffer_view = reg.view<Mesh, Transform, Material, Deferred, AABB>();
				for (auto e : gbuffer_view)
				{
					auto [mesh, transform, material, aabb] = gbuffer_view.get<Mesh, Transform, Material, AABB>(e);
					if (!aabb.camera_visible) continue;

					BatchParams params{};
					params.pso = material.pso; 
					batched_entities[params].push_back(e);
				}

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::GbufferPBR));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetFirstHandle());

				for (auto const& [params, entities] : batched_entities)
				{
					cmd_list->SetPipelineState(PSOCache::Get(params.pso));
					for (auto e : entities)
					{
						auto [mesh, transform, material, aabb] = gbuffer_view.get<Mesh, Transform, Material, AABB>(e);

						if (!aabb.camera_visible) continue;

						DirectX::XMMATRIX parent_transform = DirectX::XMMatrixIdentity();
						if (Relationship* relationship = reg.try_get<Relationship>(e))
						{
							if (auto* root_transform = reg.try_get<Transform>(relationship->parent)) parent_transform = root_transform->current_transform;
						}

						ObjectCBuffer object_cbuf_data{};
						object_cbuf_data.model = transform.current_transform * parent_transform;
						object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, object_cbuf_data.model));

						DynamicAllocation object_allocation = dynamic_allocator->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						object_allocation.Update(object_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

						MaterialCBuffer material_cbuf_data{};
						material_cbuf_data.albedo_factor = material.albedo_factor;
						material_cbuf_data.metallic_factor = material.metallic_factor;
						material_cbuf_data.roughness_factor = material.roughness_factor;
						material_cbuf_data.emissive_factor = material.emissive_factor;
						material_cbuf_data.alpha_cutoff = material.alpha_cutoff;
						material_cbuf_data.albedo_idx = material.albedo_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.albedo_texture) : 0;
						material_cbuf_data.normal_idx = material.normal_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.normal_texture) : 0;
						material_cbuf_data.metallic_roughness_idx = material.metallic_roughness_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.metallic_roughness_texture) : 0;
						material_cbuf_data.emissive_idx = material.emissive_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.emissive_texture) : 0;

						DynamicAllocation material_allocation = dynamic_allocator->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						material_allocation.Update(material_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);
						mesh.Draw(cmd_list);
					}
				}
			});
	}

	void GBufferPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

