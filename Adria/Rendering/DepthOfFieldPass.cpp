#include "DepthOfFieldPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{

	DepthOfFieldPass::DepthOfFieldPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName DepthOfFieldPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rg.GetBlackboard().Create<DoFBlackboardData>(params.dof_near_blur, params.dof_near, params.dof_far, params.dof_far_blur);

		RGResourceName last_resource = input;
		struct DepthOfFieldPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId blurred_input;
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};

		rg.AddPass<DepthOfFieldPassData>("DepthOfField Pass",
			[=](DepthOfFieldPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc dof_output_desc{};
				dof_output_desc.width = width;
				dof_output_desc.height = height;
				dof_output_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(DepthOfFieldOutput), dof_output_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(DepthOfFieldOutput));
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.blurred_input = builder.ReadTexture(RG_RES_NAME(BlurredDofInput), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
			},
			[=](DepthOfFieldPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::DOF));

				uint32 i = (uint32)descriptor_allocator->AllocateRange(4);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyTexture(data.blurred_input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct DoFConstants
				{
					XMFLOAT4 dof_params;
					uint32 depth_idx;
					uint32 scene_idx;
					uint32 blurred_idx;
					uint32 output_idx;
				} constants =
				{
					.dof_params = XMFLOAT4(params.dof_near_blur, params.dof_near, params.dof_far, params.dof_far_blur),
					.depth_idx = i, .scene_idx = i + 1, .blurred_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 8, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

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

