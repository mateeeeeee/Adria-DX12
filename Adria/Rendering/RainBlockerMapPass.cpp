#include "RainBlockerMapPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"
#include "RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"

namespace adria
{

	RainBlockerMapPass::RainBlockerMapPass(entt::registry& reg, uint32 w, uint32 h) : reg(reg), width(w), height(h)
	{
	}

	void RainBlockerMapPass::AddPass(RenderGraph& rendergraph)
	{
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		rendergraph.AddPass<void>("Rain Blocker Pass",
			[=](RenderGraphBuilder& builder)
			{
				RGTextureDesc blocker_desc{};
				blocker_desc.width = BLOCKER_DIM;
				blocker_desc.height = BLOCKER_DIM;
				blocker_desc.format = GfxFormat::R32_TYPELESS;
				blocker_desc.clear_value = GfxClearValue(1.0f, 0);
				builder.DeclareTexture(RG_RES_NAME(RainBlocker), blocker_desc);
				builder.WriteDepthStencil(RG_RES_NAME(RainBlocker), RGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(BLOCKER_DIM, BLOCKER_DIM);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				
				struct RainBlockerConstants
				{
					Matrix rain_view_projection;
				} rain_constants = 
				{
					.rain_view_projection = Matrix::Identity
				};

				cmd_list->SetRootCBV(2, rain_constants);

				auto batch_view = reg.view<Batch>();
				for (auto batch_entity : batch_view)
				{
					Batch& batch = batch_view.get<Batch>(batch_entity);

					GfxPipelineStateID pso_id = GfxPipelineStateID::RainBlocker;
					cmd_list->SetPipelineState(PSOCache::Get(pso_id));

					struct GBufferConstants
					{
						uint32 instance_id;
					} constants{ .instance_id = batch.instance_id };
					cmd_list->SetRootConstants(1, constants);

					GfxIndexBufferView ibv(batch.submesh->buffer_address + batch.submesh->indices_offset, batch.submesh->indices_count);
					cmd_list->SetTopology(batch.submesh->topology);
					cmd_list->SetIndexBuffer(&ibv);
					cmd_list->DrawIndexed(batch.submesh->indices_count);
				}
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void RainBlockerMapPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

