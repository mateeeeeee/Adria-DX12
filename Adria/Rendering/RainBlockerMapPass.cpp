#include "RainBlockerMapPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxReflection.h"
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
		CreatePSOs();
		GfxTextureDesc blocker_desc{};
		blocker_desc.width = BLOCKER_DIM;
		blocker_desc.height = BLOCKER_DIM;
		blocker_desc.format = GfxFormat::R32_TYPELESS;
		blocker_desc.clear_value = GfxClearValue(1.0f, 0);
		blocker_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::DepthStencil;
		blocker_desc.initial_state = GfxResourceState::AllSRV;
		blocker_map = std::make_unique<GfxTexture>(gfx, blocker_desc);
		blocker_map_srv = gfx->CreateTextureSRV(blocker_map.get());
	}

	void RainBlockerMapPass::AddPass(RenderGraph& rendergraph)
	{
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();

		auto [V, P] = RainBlockerMatrices(Vector4(frame_data.camera_position), BLOCKER_DIM);
		view_projection = V * P;

		rendergraph.ImportTexture(RG_NAME(RainBlocker), blocker_map.get());
		rendergraph.AddPass<void>("Rain Blocker Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteDepthStencil(RG_NAME(RainBlocker), RGLoadStoreAccessOp::Clear_Preserve);
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
					cmd_list->SetPipelineState(rain_blocker_pso.get());

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

	void RainBlockerMapPass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
		GfxReflection::FillInputLayoutDesc(GetGfxShader(VS_RainBlocker), gfx_pso_desc.input_layout);
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_RainBlocker;
		gfx_pso_desc.PS = ShaderID_Invalid;
		gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::Front;
		gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Solid;
		gfx_pso_desc.rasterizer_state.depth_bias = 7500;
		gfx_pso_desc.rasterizer_state.depth_bias_clamp = 0.0f;
		gfx_pso_desc.rasterizer_state.slope_scaled_depth_bias = 1.0f;
		gfx_pso_desc.depth_state.depth_enable = true;
		gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::LessEqual;
		gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
		rain_blocker_pso = gfx->CreateGraphicsPipelineState(gfx_pso_desc);
	}

}

