#include <map>
#include "GBufferPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
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
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), ERGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferEmissive), ERGLoadStoreAccessOp::Clear_Preserve);
				
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

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.new_frame_cbuffer_address);

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

							//later add uint materialIdx and remove these parameters -> index to material array buffer with these params
							XMFLOAT3 ambient;
							XMFLOAT3 diffuse;
							float  alpha_cutoff;
							XMFLOAT3 specular;
							float  shininess;
							float  albedo_factor;
							float  metallic_factor;
							float  roughness_factor;
							float  emissive_factor;
						} model_cbuf_data = {};

						model_cbuf_data.model_matrix = transform.current_transform * parent_transform;
						model_cbuf_data.transposed_inverse_model_matrix = XMMatrixTranspose(XMMatrixInverse(nullptr, model_cbuf_data.model_matrix));
						model_cbuf_data.diffuse = material.diffuse;
						model_cbuf_data.albedo_factor = material.albedo_factor;
						model_cbuf_data.metallic_factor = material.metallic_factor;
						model_cbuf_data.roughness_factor = material.roughness_factor;
						model_cbuf_data.emissive_factor = material.emissive_factor;
						model_cbuf_data.alpha_cutoff = material.alpha_cutoff;
						model_cbuf_data.albedo_idx = static_cast<int32>(material.albedo_texture);
						model_cbuf_data.normal_idx = static_cast<int32>(material.normal_texture);
						model_cbuf_data.metallic_roughness_idx = static_cast<int32>(material.metallic_roughness_texture);
						model_cbuf_data.emissive_idx = static_cast<int32>(material.emissive_texture);

						DynamicAllocation object_allocation = dynamic_allocator->Allocate(GetCBufferSize<ModelCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						object_allocation.Update(model_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(2, object_allocation.gpu_address);

						mesh.Draw(cmd_list);
					}
				}
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void GBufferPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

