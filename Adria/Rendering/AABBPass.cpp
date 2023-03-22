#include "AABBPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "Enums.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../Graphics/GfxLinearDynamicAllocator.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
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
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto aabb_view = reg.view<AABB>();
				for (auto e : aabb_view)
				{
					auto& aabb = aabb_view.get<AABB>(e);
					if (aabb.draw_aabb)
					{
						
						cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Solid_Wireframe));
						cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);

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
						cmd_list->SetRootCBV(2, constants);
						cmd_list->SetTopology(GfxPrimitiveTopology::LineList);
						BindVertexBuffer(cmd_list->GetNative(), aabb.aabb_vb.get());
						BindIndexBuffer(cmd_list->GetNative(), aabb_ib.get());
						cmd_list->DrawIndexed(aabb_ib->GetCount());
						aabb.draw_aabb = false;
						break;
					}
				}
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void AABBPass::OnSceneInitialized(GfxDevice* gfx)
	{
		CreateIndexBuffer(gfx);
	}

	void AABBPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void AABBPass::CreateIndexBuffer(GfxDevice* gfx)
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
		aabb_ib = std::make_unique<GfxBuffer>(gfx, IndexBufferDesc(ARRAYSIZE(aabb_indices), true), aabb_indices);
	}
}

