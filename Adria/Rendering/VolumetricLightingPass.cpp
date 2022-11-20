#include "VolumetricLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Logging/Logger.h"
#include "pix3.h"

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

		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<LightingPassData>("Volumetric Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				data.output = builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyTexture(data.depth),
															  context.GetReadWriteTexture(data.output) };
				uint32 src_range_sizes[] = { 1,1 };
				uint32 i = (uint32)descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(i);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct VolumetricLightingConstants
				{
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};
				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::VolumetricLighting));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 2, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute);

	}

}




