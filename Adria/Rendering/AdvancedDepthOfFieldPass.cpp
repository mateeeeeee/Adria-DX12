#include "AdvancedDepthOfFieldPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "PostProcessor.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<float> MaxCircleOfConfusion("r.AdvancedDepthOfField.MaxCoC", 0.01f, "Maximum value of Circle of Confusion in Advanced Depth of Field effect");
	static TAutoConsoleVariable<bool>  TemporalSmoothing("r.AdvancedDepthOfField.TemporalSmoothing", false, "Depth of Field temporal smoothing: 0 - disable, 1 - enable");

	AdvancedDepthOfFieldPass::AdvancedDepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}

	void AdvancedDepthOfFieldPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		RGResourceName color_input = postprocessor->GetFinalResource();
		AddComputeCircleOfConfusionPass(rg);
		AddDownsampleCircleOfConfusionPass(rg);
	}

	void AdvancedDepthOfFieldPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	bool AdvancedDepthOfFieldPass::IsEnabled(PostProcessor const*) const
	{
		return true;
	}

	void AdvancedDepthOfFieldPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNode("Advanced Depth Of Field"))
				{
					ImGui::SliderFloat("Max Circle of Confusion", MaxCircleOfConfusion.GetPtr(), 0.005f, 0.02f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_DepthOfField);
	}

	void AdvancedDepthOfFieldPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_DepthOfField_ComputeCoC;
		compute_coc_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_DepthOfField_DownsampleCoC;
		downsample_coc_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	void AdvancedDepthOfFieldPass::AddComputeCircleOfConfusionPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct ComputeCircleOfConfusionPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};

		rg.AddPass<ComputeCircleOfConfusionPassData>("Compute Circle Of Confusion Pass",
			[=](ComputeCircleOfConfusionPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc compute_coc_desc{};
				compute_coc_desc.width = width;
				compute_coc_desc.height = height;
				compute_coc_desc.format = GfxFormat::R16_FLOAT;

				builder.DeclareTexture(RG_NAME(ComputeCoCOutput), compute_coc_desc);
				data.output = builder.WriteTexture(RG_NAME(ComputeCoCOutput));
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](ComputeCircleOfConfusionPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(compute_coc_pso.get());

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct ComputeCircleOfConfusionPassConstants
				{
					uint32 depth_idx;
					uint32 output_idx;
					float camera_focal_length;
					float camera_focus_distance;
					float camera_aperture_ratio;
					float camera_sensor_width;
					float max_circle_of_confusion;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1,
					.camera_focal_length = 50.0f,		//move these params to camera later
					.camera_focus_distance = 200.0f,
					.camera_aperture_ratio = 5.6f,
					.camera_sensor_width = 36.0f,
					.max_circle_of_confusion = MaxCircleOfConfusion.Get()
				};

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
				GUI_DebugTexture("CoC", &ctx.GetTexture(*data.output));
			}, RGPassType::Compute, RGPassFlags::None);

	}

	void AdvancedDepthOfFieldPass::AddDownsampleCircleOfConfusionPass(RenderGraph& rg)
	{
		static constexpr uint32 pass_count = 4;

		std::vector<RGResourceName> coc_mips(pass_count);
		coc_mips[0] = RG_NAME(ComputeCoCOutput);
		for (uint32 i = 1; i < pass_count; ++i)
		{
			coc_mips[i] = RG_NAME_IDX(CoCMip, i);

			uint32 mip_width = width >> i;
			uint32 mip_height = height >> i;

			struct DownsampleCircleOfConfusionPassData
			{
				RGTextureReadOnlyId input;
				RGTextureReadWriteId output;
			};

			rg.AddPass<DownsampleCircleOfConfusionPassData>("Downsample Circle Of Confusion Pass",
				[=](DownsampleCircleOfConfusionPassData& data, RenderGraphBuilder& builder)
				{
					RGTextureDesc compute_separated_coc_desc{};
					compute_separated_coc_desc.width = mip_width;
					compute_separated_coc_desc.height = mip_height;
					compute_separated_coc_desc.format = GfxFormat::R16_UNORM;

					builder.DeclareTexture(coc_mips[i], compute_separated_coc_desc);
					data.output = builder.WriteTexture(coc_mips[i]);
					data.input = builder.ReadTexture(coc_mips[i - 1], ReadAccess_NonPixelShader);
				},
				[=](DownsampleCircleOfConfusionPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();

					cmd_list->SetPipelineState(downsample_coc_pso.get());

					GfxDescriptor src_descriptors[] =
					{
						ctx.GetReadOnlyTexture(data.input),
						ctx.GetReadWriteTexture(data.output)
					};
					GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
					gfx->CopyDescriptors(dst_descriptor, src_descriptors);
					uint32 const i = dst_descriptor.GetIndex();

					struct DownsampleCircleOfConfusionPassConstants
					{
						uint32 input_idx;
						uint32 output_idx;
					} constants =
					{
						.input_idx = i, .output_idx = i + 1,
					};

					cmd_list->SetRootConstants(1, constants);
					cmd_list->Dispatch(DivideAndRoundUp(mip_width, 16), DivideAndRoundUp(mip_height, 16), 1);

				}, RGPassType::Compute, RGPassFlags::ForceNoCull);

		}
	}

}

