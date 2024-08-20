#include "DepthOfFieldPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "PostProcessor.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Math/Constants.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<float> MaxCircleOfConfusion("r.DepthOfField.MaxCoC", 0.01f, "Maximum value of Circle of Confusion in Custom Depth of Field effect");
	static TAutoConsoleVariable<int>   BokehKernelRingCount("r.DepthOfField.Bokeh.KernelRingCount", 5, "");
	static TAutoConsoleVariable<int>   BokehKernelRingDensity("r.DepthOfField.Bokeh.KernelRingDensity", 7, "");
	static TAutoConsoleVariable<bool>  BokehKarisInverse("r.DepthOfField.Bokeh.KarisInverse", false, "Karis Inverse: 0 - disable, 1 - enable");

	static constexpr uint32 SMALL_BOKEH_KERNEL_RING_COUNT   = 3;
	static constexpr uint32 SMALL_BOKEH_KERNEL_RING_DENSITY = 5;

	static uint32 GetSampleCount(uint32 ring_count, uint32 ring_density)
	{
		return 1 + ring_density * (ring_count - 1) * ring_count / 2;
	}
	static std::vector<Vector2> GenerateKernel(uint32 ring_count, uint32 ring_density)
	{
		uint32 sample_count = GetSampleCount(ring_count, ring_density);
		std::vector<Vector2> kernel_data{};
		kernel_data.reserve(sample_count);

		float radius_increment = 1.0f / ((static_cast<float>(ring_count) - 1.0f));
		for (int32 i = ring_count - 1; i >= 0; --i)
		{
			uint32 point_count = std::max(ring_density * i, 1u);
			float radius = static_cast<float>(i) * radius_increment;

			float theta_increment = 2.0f * pi<float> / static_cast<float>(point_count);
			float offset = 0.1f * static_cast<float>(i);

			for (uint32 j = 0; j < point_count; ++j)
			{
				float  theta = offset + static_cast<float>(j) * theta_increment;
				kernel_data.push_back(radius * Vector2(cos(theta), sin(theta)));
			}
		}
		return kernel_data;
	}

	DepthOfFieldPass::DepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), blur_pass(gfx)
	{
		CreatePSOs();
	}

	void DepthOfFieldPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		AddComputeCircleOfConfusionPass(rg);
		AddDownsampleCircleOfConfusionPass(rg);
		AddComputePrefilteredTexturePass(rg, postprocessor->GetFinalResource());
		AddBokehFirstPass(rg, postprocessor->GetFinalResource());
		AddBokehSecondPass(rg);
	}

	void DepthOfFieldPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	bool DepthOfFieldPass::IsEnabled(PostProcessor const*) const
	{
		return true;
	}

	void DepthOfFieldPass::OnSceneInitialized()
	{
		CreateSmallBokehKernel();
		CreateLargeBokehKernel();
	}

	void DepthOfFieldPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNode("Custom Depth Of Field"))
				{
					ImGui::SliderFloat("Max Circle of Confusion", MaxCircleOfConfusion.GetPtr(), 0.005f, 0.02f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_DepthOfField);
	}

	void DepthOfFieldPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_DepthOfField_ComputeCoC;
		compute_coc_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_DepthOfField_DownsampleCoC;
		downsample_coc_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_DepthOfField_ComputePrefilteredTexture;
		compute_prefiltered_texture_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_DepthOfField_BokehFirstPass;
		bokeh_first_pass_psos.Initialize(compute_pso_desc);
		bokeh_first_pass_psos.AddDefine<1>("KARIS_INVERSE", "1");
		bokeh_first_pass_psos.Finalize(gfx);
	}

	void DepthOfFieldPass::CreateSmallBokehKernel()
	{
		std::vector<Vector2> bokeh_kernel_data = GenerateKernel(SMALL_BOKEH_KERNEL_RING_COUNT, SMALL_BOKEH_KERNEL_RING_DENSITY);

		GfxTextureSubData data{};
		data.data = bokeh_kernel_data.data();
		data.row_pitch = bokeh_kernel_data.size() * sizeof(float) * 2;
		data.slice_pitch = 0;

		GfxTextureData init_data{};
		init_data.sub_data = &data;
		init_data.sub_count = 1;

		GfxTextureDesc kernel_desc{};
		kernel_desc.width = (uint32)bokeh_kernel_data.size();
		kernel_desc.height = 1;
		kernel_desc.format = GfxFormat::R32_FLOAT;
		kernel_desc.initial_state = GfxResourceState::PixelSRV;
		kernel_desc.bind_flags = GfxBindFlag::ShaderResource;

		bokeh_small_kernel = gfx->CreateTexture(kernel_desc, init_data);
		bokeh_small_kernel->SetName("Bokeh Small Kernel");
	}

	void DepthOfFieldPass::CreateLargeBokehKernel()
	{
		uint32 const ring_density = BokehKernelRingDensity.Get();
		uint32 const ring_count   = BokehKernelRingCount.Get();

		std::vector<Vector2> bokeh_kernel_data = GenerateKernel(ring_count, ring_density);

		GfxTextureSubData data{};
		data.data = bokeh_kernel_data.data();
		data.row_pitch = bokeh_kernel_data.size() * sizeof(float) * 2;
		data.slice_pitch = 0;

		GfxTextureData init_data{};
		init_data.sub_data = &data;
		init_data.sub_count = 1;

		GfxTextureDesc kernel_desc{};
		kernel_desc.width = (uint32)bokeh_kernel_data.size();
		kernel_desc.height = 1;
		kernel_desc.format = GfxFormat::R32_FLOAT;
		kernel_desc.initial_state = GfxResourceState::PixelSRV;
		kernel_desc.bind_flags = GfxBindFlag::ShaderResource;

		bokeh_large_kernel = gfx->CreateTexture(kernel_desc, init_data);
		bokeh_large_kernel->SetName("Bokeh Large Kernel");
	}

	void DepthOfFieldPass::AddComputeCircleOfConfusionPass(RenderGraph& rg)
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

				builder.DeclareTexture(RG_NAME(CoCTexture), compute_coc_desc);
				data.output = builder.WriteTexture(RG_NAME(CoCTexture));
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

	void DepthOfFieldPass::AddDownsampleCircleOfConfusionPass(RenderGraph& rg)
	{
		static constexpr uint32 pass_count = 4;

		std::vector<RGResourceName> coc_mips(pass_count);
		coc_mips[0] = RG_NAME(CoCTexture);
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

				}, RGPassType::Compute, RGPassFlags::None);
		}
		blur_pass.AddPass(rg, coc_mips.back(), RG_NAME(CoCDilation), "CoC Blur");
	}

	void DepthOfFieldPass::AddComputePrefilteredTexturePass(RenderGraph& rg, RGResourceName color_texture)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct ComputePrefilteredTexturePassData
		{
			RGTextureReadOnlyId color;
			RGTextureReadOnlyId coc;
			RGTextureReadOnlyId coc_dilation;
			RGTextureReadWriteId near_coc;
			RGTextureReadWriteId far_coc;
		};

		rg.AddPass<ComputePrefilteredTexturePassData>("Compute Prefiltered Texture Pass",
			[=](ComputePrefilteredTexturePassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc prefiltered_texture_desc{};
				prefiltered_texture_desc.width = width / 2;
				prefiltered_texture_desc.height = height / 2;
				prefiltered_texture_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_NAME(NearCoC), prefiltered_texture_desc);
				builder.DeclareTexture(RG_NAME(FarCoC), prefiltered_texture_desc);

				data.near_coc = builder.WriteTexture(RG_NAME(NearCoC));
				data.far_coc = builder.WriteTexture(RG_NAME(FarCoC));
				data.color = builder.ReadTexture(color_texture, ReadAccess_NonPixelShader);
				data.coc = builder.ReadTexture(RG_NAME(CoCTexture), ReadAccess_NonPixelShader);
				data.coc_dilation = builder.ReadTexture(RG_NAME(CoCDilation), ReadAccess_NonPixelShader);
			},
			[=](ComputePrefilteredTexturePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(compute_prefiltered_texture_pso.get());

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.color),
					ctx.GetReadOnlyTexture(data.coc),
					ctx.GetReadOnlyTexture(data.coc_dilation),
					ctx.GetReadWriteTexture(data.near_coc),
					ctx.GetReadWriteTexture(data.far_coc)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct ComputePrefilteredTextureConstants
				{
					uint32 color_idx;
					uint32 coc_idx;
					uint32 coc_dilation_idx;
					uint32 foreground_output_idx;
					uint32 background_output_idx;
				} constants =
				{
					.color_idx = i, 
					.coc_idx = i + 1,
					.coc_dilation_idx = i + 2,		
					.foreground_output_idx = i + 3,
					.background_output_idx = i + 4,
				};

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width / 2, 16), DivideAndRoundUp(height / 2, 16), 1);

				GUI_DebugTexture("CoC Dilation",   &ctx.GetTexture(*data.coc_dilation));
				GUI_DebugTexture("CoC Foreground", &ctx.GetTexture(*data.near_coc));
				GUI_DebugTexture("CoC Background", &ctx.GetTexture(*data.far_coc));
			}, RGPassType::Compute, RGPassFlags::ForceNoCull);

	}

	void DepthOfFieldPass::AddBokehFirstPass(RenderGraph& rg, RGResourceName color_texture)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.ImportTexture(RG_NAME(BokehLargeKernel), bokeh_large_kernel.get());

		struct BokehFirstPassData
		{
			RGTextureReadOnlyId color;
			RGTextureReadOnlyId kernel;
			RGTextureReadOnlyId coc_near;
			RGTextureReadOnlyId coc_far;
			RGTextureReadWriteId output0;
			RGTextureReadWriteId output1;
		};

		rg.AddPass<BokehFirstPassData>("Bokeh First Pass",
			[=](BokehFirstPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc bokeh_desc{};
				bokeh_desc.width = width / 2;
				bokeh_desc.height = height / 2;
				bokeh_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(BokehTexture0), bokeh_desc);
				builder.DeclareTexture(RG_NAME(BokehTexture1), bokeh_desc);

				data.output0 = builder.WriteTexture(RG_NAME(BokehTexture0));
				data.output1 = builder.WriteTexture(RG_NAME(BokehTexture1));
				data.kernel = builder.ReadTexture(RG_NAME(BokehLargeKernel));
				data.color = builder.ReadTexture(color_texture);
				data.coc_near = builder.ReadTexture(RG_NAME(NearCoC));
				data.coc_far = builder.ReadTexture(RG_NAME(FarCoC));
			},
			[=](BokehFirstPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(compute_coc_pso.get());

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.color),
					ctx.GetReadOnlyTexture(data.kernel),
					ctx.GetReadOnlyTexture(data.coc_near),
					ctx.GetReadOnlyTexture(data.coc_far),
					ctx.GetReadWriteTexture(data.output0),
					ctx.GetReadWriteTexture(data.output1)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct BokehFirstPassConstants
				{
					uint32 color_idx;
					uint32 kernel_idx;
					uint32 coc_near_idx;
					uint32 coc_far_idx;
					uint32 output0_idx;
					uint32 output1_idx;
					uint32 sample_count;
					float  max_coc;
				} constants =
				{
					.color_idx = i,
					.kernel_idx = i + 1,
					.coc_near_idx = i + 2,
					.coc_far_idx = i + 3,
					.output0_idx = i + 4,			
					.output1_idx = i + 5,
					.sample_count = GetSampleCount(BokehKernelRingCount.Get(), BokehKernelRingDensity.Get()),
					.max_coc = MaxCircleOfConfusion.Get()
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width / 2, 16), DivideAndRoundUp(height / 2, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void DepthOfFieldPass::AddBokehSecondPass(RenderGraph& rg)
	{

	}

}

