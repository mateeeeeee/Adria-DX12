#include "VolumetricLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Logging/Logger.h"


using namespace DirectX;

namespace adria
{

	void VolumetricLightingPass::AddPass(RenderGraph& rendergraph)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();
		rendergraph.AddPass<LightingPassData>("Volumetric Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				data.output = builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				
				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.depth),
												context.GetReadWriteTexture(data.output) };
				GfxDescriptor dst_handle = descriptor_allocator->Allocate(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct VolumetricLightingConstants
				{
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};
				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::VolumetricLighting));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute);

	}

}




