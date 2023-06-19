#include <map>
#include "GBufferPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
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
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				auto GetPSO = [](MaterialAlphaMode alpha_mode)
				{
					switch (alpha_mode)
					{
					case MaterialAlphaMode::Opaque: return GfxPipelineStateID::GBuffer;
					case MaterialAlphaMode::Mask: return GfxPipelineStateID::GBuffer_Mask;
					case MaterialAlphaMode::Blend: return GfxPipelineStateID::GBuffer_NoCull;
					}
					return GfxPipelineStateID::GBuffer;
				};

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);

				reg.sort<Batch>([](Batch const& lhs, Batch const& rhs) { return lhs.alpha_mode < rhs.alpha_mode; });
				auto batch_view = reg.view<Batch>();
				for (auto batch_entity : batch_view)
				{
					Batch& batch = batch_view.get<Batch>(batch_entity);
					if (!batch.camera_visibility) continue;

					GfxPipelineStateID pso_id = GetPSO(batch.alpha_mode);
					cmd_list->SetPipelineState(PSOCache::Get(pso_id));

					struct GBufferConstants
					{
						uint32 instance_id;
					} constants { .instance_id = batch.instance_id };
					cmd_list->SetRootConstants(1, constants);

					GfxIndexBufferView ibv(batch.submesh->buffer_address + batch.submesh->indices_offset, batch.submesh->indices_count);
					cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
					cmd_list->SetIndexBuffer(&ibv);
					cmd_list->DrawIndexed(batch.submesh->indices_count);
				}
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void GBufferPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

