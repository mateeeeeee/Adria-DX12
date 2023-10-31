#include "DepthOfFieldPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{

	DepthOfFieldPass::DepthOfFieldPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName DepthOfFieldPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
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
				dof_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(DepthOfFieldOutput), dof_output_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(DepthOfFieldOutput));
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.blurred_input = builder.ReadTexture(RG_RES_NAME(BlurredDofInput), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
			},
			[=](DepthOfFieldPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DOF));

				uint32 i = gfx->AllocateDescriptorsGPU(4).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.depth));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadOnlyTexture(data.input));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 2), ctx.GetReadOnlyTexture(data.blurred_input));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 3), ctx.GetReadWriteTexture(data.output));

				struct DoFConstants
				{
					Vector4 dof_params;
					uint32 depth_idx;
					uint32 scene_idx;
					uint32 blurred_idx;
					uint32 output_idx;
				} constants =
				{
					.dof_params = Vector4(params.dof_near_blur, params.dof_near, params.dof_far, params.dof_far_blur),
					.depth_idx = i, .scene_idx = i + 1, .blurred_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		GUI_RunCommand([&]()
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

