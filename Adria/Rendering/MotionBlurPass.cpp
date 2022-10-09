#include "MotionBlurPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	MotionBlurPass::MotionBlurPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName MotionBlurPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = input;
		struct MotionBlurPassData
		{
			RGTextureReadOnlyId input_srv;
			RGTextureReadOnlyId velocity_srv;
		};
		rg.AddPass<MotionBlurPassData>("Motion Blur Pass",
			[=](MotionBlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc motion_blur_desc{};
				motion_blur_desc.width = width;
				motion_blur_desc.height = height;
				motion_blur_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.SetViewport(width, height);
				builder.DeclareTexture(RG_RES_NAME(MotionBlurOutput), motion_blur_desc);
				builder.WriteRenderTarget(RG_RES_NAME(MotionBlurOutput), ERGLoadStoreAccessOp::Discard_Preserve);
				data.input_srv = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.velocity_srv = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_PixelShader);
			},
			[=](MotionBlurPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::MotionBlur));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::MotionBlur));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.input_srv), context.GetReadOnlyTexture(data.velocity_srv) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1 };
				uint32 dst_range_sizes[] = { 2 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);

		return RG_RES_NAME(MotionBlurOutput);
	}

	void MotionBlurPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

