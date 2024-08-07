#include "DeferredLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxCommon.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Math/Packing.h"
#include "Logging/Logger.h"

using namespace DirectX;

namespace adria
{

	DeferredLightingPass::DeferredLightingPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}

	void DeferredLightingPass::AddPass(RenderGraph& rg)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  gbuffer_emissive;
			RGTextureReadOnlyId  depth;
			RGTextureReadOnlyId  ambient_occlusion;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<LightingPassData>("Deferred Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hdr_desc{};
				hdr_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				hdr_desc.width = width;
				hdr_desc.height = height;
				hdr_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_NAME(HDR_RenderTarget), hdr_desc);

				data.output			  = builder.WriteTexture(RG_NAME(HDR_RenderTarget));
				data.gbuffer_normal   = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo   = builder.ReadTexture(RG_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
				data.depth			  = builder.ReadTexture(RG_NAME(DepthStencil),  ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_NAME(AmbientOcclusion))) data.ambient_occlusion = builder.ReadTexture(RG_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				else data.ambient_occlusion.Invalidate();

				for (auto& shadow_texture : shadow_textures) std::ignore = builder.ReadTexture(shadow_texture);
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal),
												context.GetReadOnlyTexture(data.gbuffer_albedo),
												context.GetReadOnlyTexture(data.depth),
												context.GetReadOnlyTexture(data.gbuffer_emissive),
												data.ambient_occlusion.IsValid() ? context.GetReadOnlyTexture(data.ambient_occlusion) : gfxcommon::GetCommonView(GfxCommonViewType::WhiteTexture2D_SRV),
												context.GetReadWriteTexture(data.output) };

				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				uint32 i = dst_handle.GetIndex();
				gfx->CopyDescriptors(dst_handle, src_handles);

				float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearUAV(context.GetTexture(*data.output), gfx->GetDescriptorGPU(i + 5),
					context.GetReadWriteTexture(data.output), clear);

				struct DeferredLightingConstants
				{
					uint32 normal_metallic_idx;
					uint32 diffuse_idx;
					uint32 depth_idx;
					uint32 emissive_idx;
					uint32 ao_idx;
					uint32 output_idx;
				} constants =
				{
					.normal_metallic_idx = i, .diffuse_idx = i + 1, .depth_idx = i + 2, .emissive_idx = i + 3, .ao_idx = i + 4, .output_idx = i + 5
				};

				cmd_list->SetPipelineState(deferred_lighting_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);

		shadow_textures.clear();
	}

	void DeferredLightingPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_DeferredLighting;
		deferred_lighting_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

