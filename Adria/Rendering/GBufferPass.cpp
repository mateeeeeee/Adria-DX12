#include <map>
#include "GBufferPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../Graphics/LinearDynamicAllocator.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	GBufferPass::GBufferPass(entt::registry& reg, uint32 w, uint32 h) :
		reg{ reg }, width{ w }, height{ h }
	{}

	void GBufferPass::AddPass(RenderGraph& rendergraph)
	{
		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();
		rendergraph.AddPass<void>("GBuffer Pass",
			[=](RenderGraphBuilder& builder)
			{
				RGTextureDesc gbuffer_desc{};
				gbuffer_desc.width = width;
				gbuffer_desc.height = height;
				gbuffer_desc.format = GfxFormat::R8G8B8A8_UNORM;
				gbuffer_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				
				builder.DeclareTexture(RG_RES_NAME(GBufferNormal), gbuffer_desc);
				builder.DeclareTexture(RG_RES_NAME(GBufferAlbedo), gbuffer_desc);
				builder.DeclareTexture(RG_RES_NAME(GBufferEmissive), gbuffer_desc);

				builder.WriteRenderTarget(RG_RES_NAME(GBufferNormal), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferEmissive), RGLoadStoreAccessOp::Clear_Preserve);
				
				RGTextureDesc depth_desc{};
				depth_desc.width = width;
				depth_desc.height = height;
				depth_desc.format = GfxFormat::R32_TYPELESS;
				depth_desc.clear_value = GfxClearValue(1.0f, 0);
				builder.DeclareTexture(RG_RES_NAME(DepthStencil), depth_desc);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();
				
				struct BatchParams
				{
					GfxPipelineStateID pso;
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

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);

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

						struct ModelCBuffer
						{
							XMMATRIX model_matrix;
							XMMATRIX transposed_inverse_model_matrix;

							uint32 albedo_idx;
							uint32 normal_idx;
							uint32 metallic_roughness_idx;
							uint32 emissive_idx;

							XMFLOAT3 base_color;
							float  metallic_factor;
							float  roughness_factor;
							float  emissive_factor;
							float  alpha_cutoff;
						} model_cbuf_data = {};

						model_cbuf_data.model_matrix = transform.current_transform * parent_transform;
						model_cbuf_data.transposed_inverse_model_matrix = XMMatrixTranspose(XMMatrixInverse(nullptr, model_cbuf_data.model_matrix));
						model_cbuf_data.base_color = XMFLOAT3(material.base_color);
						model_cbuf_data.metallic_factor = material.metallic_factor;
						model_cbuf_data.roughness_factor = material.roughness_factor;
						model_cbuf_data.emissive_factor = material.emissive_factor;
						model_cbuf_data.alpha_cutoff = material.alpha_cutoff;
						model_cbuf_data.albedo_idx = static_cast<int32>(material.albedo_texture);
						model_cbuf_data.normal_idx = static_cast<int32>(material.normal_texture);
						model_cbuf_data.metallic_roughness_idx = static_cast<int32>(material.metallic_roughness_texture);
						model_cbuf_data.emissive_idx = static_cast<int32>(material.emissive_texture);

						DynamicAllocation model_allocation = dynamic_allocator->Allocate(GetCBufferSize<ModelCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						model_allocation.Update(model_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(2, model_allocation.gpu_address);

						mesh.Draw(cmd_list);
					}
				}
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void GBufferPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

