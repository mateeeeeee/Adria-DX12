#include "RainBlockerMapPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxTexture.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;
namespace adria
{
	static std::pair<Matrix, Matrix> RainBlockerMatrices(Vector4 const& camera_position, uint32 map_size)
	{
		Vector3 const max_extents(50.0f, 50.0f, 50.0f);
		Vector3 const min_extents = -max_extents;

		float l = min_extents.x;
		float b = min_extents.y;
		float n = 1.0f;
		float r = max_extents.x;
		float t = max_extents.y;
		float f = (1000 - camera_position.y) * 1.5f;
		Matrix V = XMMatrixLookAtLH(Vector4(0, 1000, 0, 1), camera_position, Vector3::Forward);
		Matrix P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
		return { V,P };
	}


	RainBlockerMapPass::RainBlockerMapPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h) : reg(reg), gfx(gfx), width(w), height(h)
	{
		GfxTextureDesc blocker_desc{};
		blocker_desc.width = BLOCKER_DIM;
		blocker_desc.height = BLOCKER_DIM;
		blocker_desc.format = GfxFormat::R32_TYPELESS;
		blocker_desc.clear_value = GfxClearValue(1.0f, 0);
		blocker_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::DepthStencil;
		blocker_desc.initial_state = GfxResourceState::AllShaderResource;
		blocker_map = std::make_unique<GfxTexture>(gfx, blocker_desc);
		blocker_map_srv = gfx->CreateTextureSRV(blocker_map.get());
	}

	void RainBlockerMapPass::AddPass(RenderGraph& rendergraph)
	{
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();

		auto [V, P] = RainBlockerMatrices(Vector4(frame_data.camera_position), BLOCKER_DIM);
		view_projection = V * P;

		rendergraph.ImportTexture(RG_RES_NAME(RainBlocker), blocker_map.get());
		rendergraph.AddPass<void>("Rain Blocker Pass",
			[=](RenderGraphBuilder& builder)
			{
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
					.rain_view_projection = view_projection
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

			}, RGPassType::Graphics, RGPassFlags::ForceNoCull);
	}

	void RainBlockerMapPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	int32 RainBlockerMapPass::GetRainBlockerMapIdx() const
	{
		GfxDescriptor blocker_map_srv_gpu = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, blocker_map_srv_gpu, blocker_map_srv);
		return (int32)blocker_map_srv_gpu.GetIndex();
	}

}

