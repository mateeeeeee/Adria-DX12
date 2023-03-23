#include "AmbientPass.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"

#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GfxCommon.h"
#include "../Graphics/GfxLinearDynamicAllocator.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../Math/Packing.h"
#include "../Editor/GUICommand.h"

namespace adria
{
	AmbientPass::AmbientPass(uint32 w, uint32 h)
		: width(w), height(h)
	{}

	void AmbientPass::AddPass(RenderGraph& rendergraph)
	{
		struct AmbientPassData
		{
			RGTextureReadOnlyId gbuffer_normal;
			RGTextureReadOnlyId gbuffer_albedo;
			RGTextureReadOnlyId gbuffer_emissive;
			RGTextureReadOnlyId depth_stencil;

			RGTextureReadOnlyId ambient_occlusion;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();
		rendergraph.AddPass<AmbientPassData>("Ambient Pass",
			[=](AmbientPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hdr_desc{};
				hdr_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				hdr_desc.width = width;
				hdr_desc.height = height;
				hdr_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(HDR_RenderTarget), hdr_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_RES_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
				data.depth_stencil = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				if (builder.IsTextureDeclared(RG_RES_NAME(AmbientOcclusion)))
					data.ambient_occlusion = builder.ReadTexture(RG_RES_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				else
					data.ambient_occlusion.Invalidate();
			},
			[&](AmbientPassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal),
					context.GetReadOnlyTexture(data.gbuffer_albedo), context.GetReadOnlyTexture(data.gbuffer_emissive), context.GetReadOnlyTexture(data.depth_stencil),
					data.ambient_occlusion.IsValid() ? context.GetReadOnlyTexture(data.ambient_occlusion) : gfxcommon::GetCommonView(GfxCommonViewType::NullTexture2D_SRV),
					context.GetReadWriteTexture(data.output) };

				GfxDescriptor dst_handle = descriptor_allocator->Allocate(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearUAV(context.GetTexture(*data.output), descriptor_allocator->GetHandle(i + 5),
					context.GetReadWriteTexture(data.output), clear);

				struct AmbientConstants
				{
					uint32  color;

					uint32  normal_idx;
					uint32  diffuse_idx;
					uint32  emissive_idx;
					uint32  depth_idx;
					int32   ao_idx;
					uint32  output_idx;
				} constants =
				{
					.color = PackToUint(ambient_color),
					.normal_idx = i, .diffuse_idx = i + 1, .emissive_idx = i + 2,
					.depth_idx = i + 3, .ao_idx = -1, .output_idx = i + 5
				};
				if (data.ambient_occlusion.IsValid()) constants.ao_idx = static_cast<int32>(i + 4);


				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Ambient));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute);

		AddGUI([&]()
			{
				if (ImGui::TreeNode("Ambient"))
				{
					ImGui::ColorEdit3("Ambient Color", ambient_color);
					ImGui::Checkbox("IBL", &ibl);
					if (ibl) ibl = false;
					ImGui::TreePop();
				}
			}
		);
	}

	void AmbientPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}