#include "AutoExposurePass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Postprocessor.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxPipelineState.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"
#include <algorithm> 

namespace adria
{
	static TAutoConsoleVariable<bool>  AutoExposure("r.AutoExposure", true, "Enable or Disable Auto Exposure");
	static TAutoConsoleVariable<float> ExposureBias("r.AutoExposure.ExposureBias", -0.5f, "Bias applied to the computed EV100 when calculating final exposure");

	AutoExposurePass::AutoExposurePass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}

	void AutoExposurePass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		struct BuildHistogramData
		{
			RGTextureReadOnlyId scene_texture;
			RGBufferReadWriteId histogram_buffer;
		};
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<BuildHistogramData>("Build Histogram Pass",
			[=](BuildHistogramData& data, RenderGraphBuilder& builder)
			{
				data.scene_texture = builder.ReadTexture(postprocessor->GetFinalResource());

				RGBufferDesc desc{};
				desc.stride = sizeof(uint32);
				desc.size = desc.stride * 256;
				desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
				desc.resource_usage = GfxResourceUsage::Default;
				builder.DeclareBuffer(RG_NAME(HistogramBuffer), desc);
				data.histogram_buffer = builder.WriteBuffer(RG_NAME(HistogramBuffer));
			},
			[=](BuildHistogramData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(2);
				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.scene_texture), context.GetReadWriteBuffer(data.histogram_buffer) };
				gfx->CopyDescriptors(dst_handle, src_handles);

				uint32 descriptor_index = dst_handle.GetIndex();
				GfxDescriptor scene_srv = gfx->GetDescriptorGPU(descriptor_index);
				GfxDescriptor buffer_gpu = gfx->GetDescriptorGPU(descriptor_index + 1);

				GfxBuffer const& histogram_buffer = context.GetBuffer(*data.histogram_buffer);
				uint32 clear_value[4] = { 0, 0, 0, 0 };
				cmd_list->ClearUAV(histogram_buffer, buffer_gpu, context.GetReadWriteBuffer(data.histogram_buffer), clear_value);
				cmd_list->BufferBarrier(histogram_buffer, GfxResourceState::ComputeUAV, GfxResourceState::ComputeUAV);
				cmd_list->FlushBarriers();
				cmd_list->SetPipelineState(build_histogram_pso.get());

				struct BuildHistogramConstants
				{
					uint32  width;
					uint32  height;
					float rcp_width;
					float rcp_height;
					float min_luminance;
					float max_luminance;
					uint32  scene_idx;
					uint32  histogram_idx;
				} constants = { .width = width, .height = height,
								.rcp_width = 1.0f / width, .rcp_height = 1.0f / height,
								.min_luminance = min_luminance, .max_luminance = max_luminance,
								.scene_idx = descriptor_index, .histogram_idx = descriptor_index + 1 };
				cmd_list->SetRootConstants(1, constants);

				auto DivideRoudingUp = [](uint32 a, uint32 b)
					{
						return (a + b - 1) / b;
					};
				cmd_list->Dispatch(DivideRoudingUp(width, 16), DivideRoudingUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		struct HistogramReductionData
		{
			RGBufferReadOnlyId histogram_buffer;
			RGTextureReadWriteId avg_luminance;
		};

		rg.AddPass<HistogramReductionData>("Histogram Reduction Pass",
			[=](HistogramReductionData& data, RenderGraphBuilder& builder)
			{
				data.histogram_buffer = builder.ReadBuffer(RG_NAME(HistogramBuffer));
				RGTextureDesc desc{};
				desc.width = desc.height = 1;
				desc.format = GfxFormat::R16_FLOAT;
				builder.DeclareTexture(RG_NAME(AverageLuminance), desc);
				data.avg_luminance = builder.WriteTexture(RG_NAME(AverageLuminance));
			},
			[=](HistogramReductionData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(histogram_reduction_pso.get());
				uint32 descriptor_index = gfx->AllocateDescriptorsGPU(2).GetIndex();

				GfxDescriptor buffer_srv = gfx->GetDescriptorGPU(descriptor_index);
				gfx->CopyDescriptors(1, buffer_srv, context.GetReadOnlyBuffer(data.histogram_buffer));
				GfxDescriptor avgluminance_uav = gfx->GetDescriptorGPU(descriptor_index + 1);
				gfx->CopyDescriptors(1, avgluminance_uav, context.GetReadWriteTexture(data.avg_luminance));

				struct HistogramReductionConstants
				{
					float min_luminance;
					float max_luminance;
					float low_percentile;
					float high_percentile;
					uint32  histogram_idx;
					uint32  luminance_idx;
				} constants = { .min_luminance = min_luminance, .max_luminance = max_luminance,
								.low_percentile = low_percentile, .high_percentile = high_percentile,
								.histogram_idx = descriptor_index, .luminance_idx = descriptor_index + 1 };
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);
			}, RGPassType::Compute, RGPassFlags::None);

		struct ExposureData
		{
			RGTextureReadOnlyId avg_luminance;
			RGTextureReadWriteId exposure;
		};
		rg.AddPass<ExposureData>("Exposure Pass",
			[&](ExposureData& data, RenderGraphBuilder& builder)
			{
				ADRIA_ASSERT(builder.IsTextureDeclared(RG_NAME(AverageLuminance)));
				data.avg_luminance = builder.ReadTexture(RG_NAME(AverageLuminance));

				RGTextureDesc desc{};
				desc.width = desc.height = 1;
				desc.format = GfxFormat::R16_FLOAT;
				builder.DeclareTexture(RG_NAME(Exposure), desc);
				data.exposure = builder.WriteTexture(RG_NAME(Exposure));
			},
			[=](ExposureData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				if (invalid_history)
				{
					GfxDescriptor cpu_descriptor = previous_ev100_uav;
					GfxDescriptor gpu_descriptor = gfx->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, gpu_descriptor, cpu_descriptor);
					float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					cmd_list->ClearUAV(*previous_ev100, gpu_descriptor, cpu_descriptor, clear_value);
					invalid_history = false;
				}

				cmd_list->SetPipelineState(exposure_pso.get());
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(3);
				GfxDescriptor src_descriptors[] = {
					previous_ev100_uav,
					context.GetReadWriteTexture(data.exposure),
					context.GetReadOnlyTexture(data.avg_luminance)
				};
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 descriptor_index = dst_descriptor.GetIndex();

				struct ExposureConstants
				{
					float adaption_speed;
					float exposure_bias;
					float frame_time;
					uint32  previous_ev_idx;
					uint32  exposure_idx;
					uint32  luminance_idx;
				} constants{ .adaption_speed = adaption_speed, .exposure_bias = ExposureBias.Get(), .frame_time = 0.166f,
						.previous_ev_idx = descriptor_index, .exposure_idx = descriptor_index + 1, .luminance_idx = descriptor_index + 2 };

				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);
			}, RGPassType::Compute, RGPassFlags::None);

		if (show_histogram) rg.ExportBuffer(RG_NAME(HistogramBuffer), histogram_copy.get());
	}

	void AutoExposurePass::OnSceneInitialized()
	{
		GfxTextureDesc desc{};
		desc.width = 1;
		desc.height = 1;
		desc.mip_levels = 1;
		desc.bind_flags = GfxBindFlag::UnorderedAccess;
		desc.misc_flags = GfxTextureMiscFlag::None;
		desc.initial_state = GfxResourceState::ComputeUAV;
		desc.format = GfxFormat::R16_FLOAT;

		previous_ev100 = gfx->CreateTexture(desc);
		previous_ev100_uav = gfx->CreateTextureUAV(previous_ev100.get());

		GfxBufferDesc hist_desc{};
		hist_desc.stride = sizeof(uint32);
		hist_desc.size = hist_desc.stride * 256;
		hist_desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		hist_desc.resource_usage = GfxResourceUsage::Readback;
		histogram_copy = gfx->CreateBuffer(hist_desc);
	}

	void AutoExposurePass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Automatic Exposure", 0))
				{
					ImGui::Checkbox("Enable", AutoExposure.GetPtr());
					if (AutoExposure.Get())
					{
						ImGui::SliderFloat("Min Luminance", &min_luminance, 0.0f, 1.0f);
						ImGui::SliderFloat("Max Luminance", &max_luminance, 0.3f, 20.0f);
						ImGui::SliderFloat("Adaption Speed", &adaption_speed, 0.01f, 5.0f);
						ImGui::SliderFloat("Exposure Bias", ExposureBias.GetPtr(), -5.0f, 5.0f);
						ImGui::SliderFloat("Low Percentile", &low_percentile, 0.0f, 0.49f);
						ImGui::SliderFloat("High Percentile", &high_percentile, 0.51f, 1.0f);
						ImGui::Checkbox("Histogram", &show_histogram);
						if (show_histogram)
						{
							ADRIA_ASSERT(histogram_copy->IsMapped());
							uint64 histogram_size = histogram_copy->GetSize() / sizeof(int32);
							int32* hist_data = histogram_copy->GetMappedData<int32>();
							int32 max_value = *std::max_element(hist_data, hist_data + histogram_size);
							auto converter = [](void* data, int32 idx)-> float
								{
									return static_cast<float>(*(((int32*)data) + idx));
								};
							ImGui::PlotHistogram("Luminance Histogram", converter, hist_data, (int32)histogram_size, 0, NULL, 0.0f, (float)max_value, ImVec2(0, 80));
						}
					}
					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessing
		);
	}

	void AutoExposurePass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	bool AutoExposurePass::IsEnabled(PostProcessor const*) const
	{
		return AutoExposure.Get();
	}

	void AutoExposurePass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_BuildHistogram;
		build_histogram_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_HistogramReduction;
		histogram_reduction_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_Exposure;
		exposure_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

