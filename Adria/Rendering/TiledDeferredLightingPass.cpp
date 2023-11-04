#include "TiledDeferredLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"
#include "Graphics/GfxCommon.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	TiledDeferredLightingPass::TiledDeferredLightingPass(entt::registry& reg, uint32 w, uint32 h) : reg(reg), width(w), height(h),
		add_textures_pass(width, height), copy_to_texture_pass(width, height)
	{}

	void TiledDeferredLightingPass::AddPass(RenderGraph& rendergraph)
	{
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();

		struct TiledDeferredLightingPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  gbuffer_emissive;
			RGTextureReadOnlyId  depth;
			RGTextureReadOnlyId  ambient_occlusion;
			RGTextureReadWriteId output;
			RGTextureReadWriteId debug_output;
		};

		rendergraph.AddPass<TiledDeferredLightingPassData>("Tiled Deferred Lighting Pass",
			[=](TiledDeferredLightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hdr_desc{};
				hdr_desc.width = width;
				hdr_desc.height = height;
				hdr_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				hdr_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_RES_NAME(HDR_RenderTarget), hdr_desc);
				builder.DeclareTexture(RG_RES_NAME(TiledDebugTarget), hdr_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.debug_output = builder.WriteTexture(RG_RES_NAME(TiledDebugTarget));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_RES_NAME(GBufferEmissive), ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_RES_NAME(AmbientOcclusion)))
					data.ambient_occlusion = builder.ReadTexture(RG_RES_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				else data.ambient_occlusion.Invalidate();

				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](TiledDeferredLightingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal),
												context.GetReadOnlyTexture(data.gbuffer_albedo),
												context.GetReadOnlyTexture(data.depth),
												context.GetReadOnlyTexture(data.gbuffer_emissive),
												data.ambient_occlusion.IsValid() ? context.GetReadOnlyTexture(data.ambient_occlusion) : gfxcommon::GetCommonView(GfxCommonViewType::WhiteTexture2D_SRV),
												context.GetReadWriteTexture(data.output),
												context.GetReadWriteTexture(data.debug_output) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);

				uint32 i = dst_handle.GetIndex();
				struct TiledLightingConstants
				{
					int32  visualize_max_lights;
					uint32 normal_idx;
					uint32 diffuse_idx;
					uint32 depth_idx;
					uint32 emissive_idx;
					uint32 ao_idx;
					uint32 output_idx;
					int32  debug_idx;
				} constants =
				{
					.visualize_max_lights = visualize_max_lights,
					.normal_idx = i, .diffuse_idx = i + 1, .depth_idx = i + 2, .emissive_idx = i + 3, .ao_idx = i + 4,
					.output_idx = i + 5, .debug_idx = visualize_tiled ? int32(i + 6) : -1
				};

				static constexpr float black[] = {0.0f,0.0f,0.0f,0.0f};
				GfxTexture const& tiled_target = context.GetTexture(*data.output);
				GfxTexture const& tiled_debug_target = context.GetTexture(*data.debug_output);
				cmd_list->ClearUAV(tiled_target, gfx->GetDescriptorGPU(i + 5), context.GetReadWriteTexture(data.output), black);
				cmd_list->ClearUAV(tiled_debug_target, gfx->GetDescriptorGPU(i + 6), context.GetReadWriteTexture(data.debug_output), black);

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::TiledDeferredLighting));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		if (visualize_tiled)  copy_to_texture_pass.AddPass(rendergraph, RG_RES_NAME(HDR_RenderTarget), RG_RES_NAME(TiledDebugTarget), BlendMode::AdditiveBlend);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx("Tiled Deferred", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Visualize Tiles", &visualize_tiled);
					if (visualize_tiled) ImGui::SliderInt("Visualize Scale", &visualize_max_lights, 1, 32);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

}

