#include "AABBPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "Enums.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "entt/entity/registry.hpp"

namespace adria
{

	AABBPass::AABBPass(entt::registry& reg, uint32 w, uint32 h)
		: reg(reg), width(w), height(h)
	{
	}

	void AABBPass::AddPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rg.AddPass<void>("Sky Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto upload_buffer = gfx->GetDynamicAllocator();

				auto aabb_view = reg.view<AABB>();
				for (auto e : aabb_view)
				{
					auto& aabb = aabb_view.get<AABB>(e);
					if (aabb.draw_aabb)
					{
						cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Forward));
						cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Solid_Wireframe));
						cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
						
						ObjectCBuffer object_cbuf_data{};
						object_cbuf_data.model = DirectX::XMMatrixIdentity();
						object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixIdentity();
						DynamicAllocation object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						object_allocation.Update(object_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

						MaterialCBuffer material_cbuf_data{};
						material_cbuf_data.diffuse = DirectX::XMFLOAT3(1, 0, 0);
						object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						object_allocation.Update(object_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(2, object_allocation.gpu_address);
						OffsetType desc_index = descriptor_allocator->Allocate();
						device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(desc_index),
							global_data.null_srv_texture2d, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetHandle(desc_index));

						cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
						BindVertexBuffer(cmd_list, aabb.aabb_vb.get());
						BindIndexBuffer(cmd_list, aabb_ib.get());
						cmd_list->DrawIndexedInstanced(aabb_ib->GetCount(), 1, 0, 0, 0);

						aabb.draw_aabb = false;
						break;
					}
				}
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void AABBPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		CreateIndexBuffer(gfx);
	}

	void AABBPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void AABBPass::CreateIndexBuffer(GraphicsDevice* gfx)
	{
		const uint16 aabb_indices[] = {
			0, 1,
			1, 2,
			2, 3,
			3, 0,

			0, 4,
			1, 5,
			2, 6,
			3, 7,

			4, 5,
			5, 6,
			6, 7,
			7, 4
		};
		aabb_ib = std::make_unique<Buffer>(gfx, IndexBufferDesc(ARRAYSIZE(aabb_indices), true), aabb_indices);
	}
}

