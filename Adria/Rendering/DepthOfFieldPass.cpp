#include "DepthOfFieldPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	DepthOfFieldPass::DepthOfFieldPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName DepthOfFieldPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = input;
		struct DepthOfFieldPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId blurred_input;
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<DepthOfFieldPassData>("DepthOfField Pass",
			[=](DepthOfFieldPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc dof_output_desc{};
				dof_output_desc.width = width;
				dof_output_desc.height = height;
				dof_output_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(DepthOfFieldOutput), dof_output_desc);
				builder.WriteRenderTarget(RG_RES_NAME(DepthOfFieldOutput), ERGLoadStoreAccessOp::Discard_Preserve);
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.blurred_input = builder.ReadTexture(RG_RES_NAME(BlurredDofInput), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](DepthOfFieldPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::DOF));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::DOF));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.input), context.GetReadOnlyTexture(data.blurred_input), context.GetReadOnlyTexture(data.depth) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1, 1 };
				uint32 dst_range_sizes[] = { 3 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);

		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Depth Of Field", 0))
				{
					ImGui::SliderFloat("DoF Near Blur", &params.dof_near_blur, 0.0f, 200.0f);
					ImGui::SliderFloat("DoF Near", &params.dof_near, params.dof_near_blur, 500.0f);
					ImGui::SliderFloat("DoF Far", &params.dof_far, params.dof_near, 1000.0f);
					ImGui::SliderFloat("DoF Far Blur", &params.dof_far_blur, params.dof_far, 1500.0f);
					ImGui::TreePop();
					ImGui::Separator();

				}
			});

		return RG_RES_NAME(DepthOfFieldOutput);
	}

	void DepthOfFieldPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

