#include "AABBPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "Enums.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	AABBPass::AABBPass(entt::registry& reg, uint32 w, uint32 h)
		: reg(reg), width(w), height(h)
	{}

	void AABBPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		rg.AddPass<void>("AABB Pass",
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
						
						cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Solid_Wireframe));
						cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);

						struct Constants
						{
							XMMATRIX model_matrix;
							XMFLOAT3 diffuse_color;
							uint32   diffuse_idx;
						} constants = 
						{
							.model_matrix = XMMatrixIdentity(),
							.diffuse_color = XMFLOAT3(1, 0, 0),
							.diffuse_idx = uint32(-1)
						};
						DynamicAllocation allocation = upload_buffer->Allocate(GetCBufferSize<Constants>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						allocation.Update(constants);
						cmd_list->SetGraphicsRootConstantBufferView(2, allocation.gpu_address);
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

