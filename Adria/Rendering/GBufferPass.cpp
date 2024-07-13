#include <map>
#include "GBufferPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxReflection.h"
#include "Graphics/GfxTracyProfiler.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	GBufferPass::GBufferPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h) :
		reg{ reg }, gfx{ gfx }, width{ w }, height{ h }
	{}

	void GBufferPass::AddPass(RenderGraph& rendergraph)
	{
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
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
				depth_desc.clear_value = GfxClearValue(0.0f, 0);
				builder.DeclareTexture(RG_RES_NAME(DepthStencil), depth_desc);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				auto GetPSO = [this](MaterialAlphaMode alpha_mode)
				{
					switch (alpha_mode)
					{
					case MaterialAlphaMode::Opaque: return gbuffer_psos.Get<0>();
					case MaterialAlphaMode::Mask: return gbuffer_psos.Get<1>();
					case MaterialAlphaMode::Blend: return gbuffer_psos.Get<3>();
					}
					return gbuffer_psos.Get<0>();
				};

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);

				reg.sort<Batch>([](Batch const& lhs, Batch const& rhs) { return lhs.alpha_mode < rhs.alpha_mode; });
				auto batch_view = reg.view<Batch>();
				for (auto batch_entity : batch_view)
				{
					Batch& batch = batch_view.get<Batch>(batch_entity);
					if (!batch.camera_visibility) continue;

					GfxPipelineState* pso = GetPSO(batch.alpha_mode);
					if (use_rain_pso) pso = gbuffer_psos.Get<4>();
					cmd_list->SetPipelineState(pso);

					struct GBufferConstants
					{
						uint32 instance_id;
					} constants { .instance_id = batch.instance_id };
					cmd_list->SetRootConstants(1, constants);

					GfxIndexBufferView ibv(batch.submesh->buffer_address + batch.submesh->indices_offset, batch.submesh->indices_count);
					cmd_list->SetTopology(batch.submesh->topology);
					cmd_list->SetIndexBuffer(&ibv);
					cmd_list->DrawIndexed(batch.submesh->indices_count);
				}
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void GBufferPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void GBufferPass::CreatePSOs()
	{
		using enum GfxShaderStage;
		GraphicsPipelineStateDesc gfx_pso_desc{};
		GfxReflection::FillInputLayoutDesc(GetGfxShader(VS_GBuffer), gfx_pso_desc.input_layout);
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_GBuffer;
		gfx_pso_desc.PS = PS_GBuffer;
		gfx_pso_desc.depth_state.depth_enable = true;
		gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		gfx_pso_desc.num_render_targets = 3u;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.rtv_formats[1] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.rtv_formats[2] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;

		gbuffer_psos.Initialize(gfx_pso_desc);
		gbuffer_psos.AddDefine<PS, 1>("MASK", "1");
		gbuffer_psos.AddDefine<PS, 2>("MASK", "1");
		gbuffer_psos.SetCullMode<2>(GfxCullMode::None);
		gbuffer_psos.SetCullMode<3>(GfxCullMode::None);
		gbuffer_psos.AddDefine<PS, 4>("RAIN", "1");
	}

}

