#include "DeferredLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"
#include "pix3.h"

using namespace DirectX;

namespace adria
{

	void DeferredLightingPass::AddPass(RenderGraph& rendergraph)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		rendergraph.AddPass<LightingPassData>("Deferred Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				data.output			= builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.depth			= builder.ReadTexture(RG_RES_NAME(DepthStencil),  ReadAccess_NonPixelShader);
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal),
												context.GetReadOnlyTexture(data.gbuffer_albedo),
												context.GetReadOnlyTexture(data.depth),
												context.GetReadWriteTexture(data.output) };

				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				uint32 i = dst_handle.GetIndex();
				gfx->CopyDescriptors(dst_handle, src_handles);

				struct DeferredLightingConstants
				{
					uint32 normal_metallic_idx;
					uint32 diffuse_idx;
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.normal_metallic_idx = i, .diffuse_idx = i + 1, .depth_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DeferredLighting));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute);

	}

}

