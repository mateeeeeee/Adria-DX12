#include "AutomaticExposurePass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GfxTexture.h"
#include "../Graphics/GfxBuffer.h"
#include "../Graphics/GfxLinearDynamicAllocator.h"
#include "../Graphics/RingGPUDescriptorAllocator.h"
#include "../Editor/GUICommand.h"

#include <algorithm> // remove this later

namespace adria
{

	AutomaticExposurePass::AutomaticExposurePass(uint32 w, uint32 h)
		: width(w), height(h)
	{}

	void AutomaticExposurePass::OnSceneInitialized(GfxDevice* gfx)
	{
		GfxTextureDesc desc{};
		desc.width = 1;
		desc.height = 1;
		desc.mip_levels = 1;
		desc.bind_flags = GfxBindFlag::UnorderedAccess;
		desc.misc_flags = GfxTextureMiscFlag::None;
		desc.initial_state = GfxResourceState::UnorderedAccess;
		desc.format = GfxFormat::R16_FLOAT;

		previous_ev100 = std::make_unique<GfxTexture>(gfx, desc);
		previous_ev100->CreateUAV();

		GfxBufferDesc hist_desc{};
		hist_desc.stride = sizeof(uint32);
		hist_desc.size = hist_desc.stride * 256;
		hist_desc.format = GfxFormat::R32_FLOAT;
		hist_desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		hist_desc.resource_usage = GfxResourceUsage::Readback;
		histogram_copy = std::make_unique<GfxBuffer>(gfx, hist_desc);
	}

	void AutomaticExposurePass::AddPasses(RenderGraph& rg, RGResourceName input)
	{
		struct BuildHistogramData
		{
			RGTextureReadOnlyId scene_texture;
			RGBufferReadWriteId histogram_buffer;
		};
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		rg.AddPass<BuildHistogramData>("Build Histogram Pass",
			[=](BuildHistogramData& data, RenderGraphBuilder& builder)
			{
				data.scene_texture = builder.ReadTexture(input);

				RGBufferDesc desc{};
				desc.stride = sizeof(uint32);
				desc.size = desc.stride * 256;
				desc.format = GfxFormat::R32_FLOAT;
				desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
				desc.resource_usage = GfxResourceUsage::Default;
				builder.DeclareBuffer(RG_RES_NAME(HistogramBuffer), desc);
				data.histogram_buffer = builder.WriteBuffer(RG_RES_NAME(HistogramBuffer));
			},
			[=](BuildHistogramData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				uint32 descriptor_index = (uint32)descriptor_allocator->AllocateRange(2);

				DescriptorHandle scene_srv = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, scene_srv, context.GetReadOnlyTexture(data.scene_texture),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				DescriptorHandle buffer_gpu = descriptor_allocator->GetHandle(descriptor_index + 1);
				device->CopyDescriptorsSimple(1, buffer_gpu, context.GetReadWriteBuffer(data.histogram_buffer), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
				GfxBuffer const& histogram_buffer = context.GetBuffer(data.histogram_buffer.GetResourceId());
				uint32 clear_value[4] = { 0, 0, 0, 0 };
				cmd_list->ClearUnorderedAccessViewUint(buffer_gpu, context.GetReadWriteBuffer(data.histogram_buffer), histogram_buffer.GetNative(), clear_value, 0, nullptr);

				D3D12_RESOURCE_BARRIER buffer_uav_barrier{};
				buffer_uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				buffer_uav_barrier.UAV.pResource = histogram_buffer.GetNative();
				cmd_list->ResourceBarrier(1, &buffer_uav_barrier);

				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BuildHistogram)); 
				uint32 half_width = (width + 1) / 2;
				uint32 half_height = (height + 1) / 2;

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
				} constants = {	.width = half_width, .height = half_height, 
								.rcp_width = 1.0f / half_width, .rcp_height = 1.0f / half_height,
								.min_luminance = min_luminance, .max_luminance = max_luminance,
								.scene_idx = descriptor_index, .histogram_idx = descriptor_index + 1 };
				cmd_list->SetComputeRoot32BitConstants(1, 8, &constants, 0);

				auto DivideRoudingUp = [](uint32 a, uint32 b)
				{
					return (a + b - 1) / b;
				};
				cmd_list->Dispatch(DivideRoudingUp(half_width, 16), DivideRoudingUp(half_height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		struct HistogramReductionData
		{
			RGBufferReadOnlyId histogram_buffer;
			RGTextureReadWriteId avg_luminance;
		};

		rg.AddPass<HistogramReductionData>("Histogram Reduction Pass",
			[=](HistogramReductionData& data, RenderGraphBuilder& builder)
			{
				data.histogram_buffer = builder.ReadBuffer(RG_RES_NAME(HistogramBuffer));
				RGTextureDesc desc{};
				desc.width = desc.height = 1;
				desc.format = GfxFormat::R16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(AverageLuminance), desc);
				data.avg_luminance = builder.WriteTexture(RG_RES_NAME(AverageLuminance));
			},
			[=](HistogramReductionData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::HistogramReduction));

				uint32 descriptor_index = (uint32)descriptor_allocator->AllocateRange(2);

				DescriptorHandle buffer_srv = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, buffer_srv, context.GetReadOnlyBuffer(data.histogram_buffer),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				DescriptorHandle avgluminance_uav = descriptor_allocator->GetHandle(descriptor_index + 1);
				device->CopyDescriptorsSimple(1, avgluminance_uav, context.GetReadWriteTexture(data.avg_luminance),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
				cmd_list->SetComputeRoot32BitConstants(1, 6, &constants, 0);

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
				ADRIA_ASSERT(builder.IsTextureDeclared(RG_RES_NAME(AverageLuminance)));
				data.avg_luminance = builder.ReadTexture(RG_RES_NAME(AverageLuminance));

				RGTextureDesc desc{};
				desc.width = desc.height = 1;
				desc.format = GfxFormat::R16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(Exposure), desc);
				data.exposure = builder.WriteTexture(RG_RES_NAME(Exposure));
			},
			[=](ExposureData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();
				if (invalid_history)
				{
					DescriptorCPU cpu_descriptor = previous_ev100->GetUAV();
					OffsetType descriptor_index = descriptor_allocator->Allocate();
					DescriptorHandle gpu_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, gpu_descriptor, cpu_descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					cmd_list->ClearUnorderedAccessViewFloat(gpu_descriptor, cpu_descriptor, previous_ev100->GetNative(), clear_value, 0, nullptr);
					invalid_history = false;
				}
				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Exposure));

				uint32 descriptor_index = (uint32)descriptor_allocator->AllocateRange(3);
				
				DescriptorHandle previous_uav = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, previous_uav, previous_ev100->GetUAV(),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				DescriptorHandle exposure_uav = descriptor_allocator->GetHandle(descriptor_index + 1);
				device->CopyDescriptorsSimple(1, exposure_uav, context.GetReadWriteTexture(data.exposure),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				DescriptorHandle avgluminance_srv = descriptor_allocator->GetHandle(descriptor_index + 2);
				device->CopyDescriptorsSimple(1, avgluminance_srv, context.GetReadOnlyTexture(data.avg_luminance),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct ExposureConstants
				{
					float adaption_speed;
					float exposure_compensation;
					float frame_time;
					uint32  previous_ev_idx;
					uint32  exposure_idx;
					uint32  luminance_idx;
				} constants{.adaption_speed = adaption_speed, .exposure_compensation = exposure_compensation, .frame_time = 0.166f,
						.previous_ev_idx = descriptor_index, .exposure_idx = descriptor_index + 1, .luminance_idx  = descriptor_index + 2};

				cmd_list->SetComputeRoot32BitConstants(1, 6, &constants, 0);
				cmd_list->Dispatch(1, 1, 1);
			}, RGPassType::Compute, RGPassFlags::None);
	
		if (show_histogram)
		{
			struct HistogramCopyData
			{
				RGBufferCopySrcId histogram;
			};
			rg.AddPass<HistogramCopyData>("Histogram Copy Pass",
				[&](HistogramCopyData& data, RenderGraphBuilder& builder)
				{
					ADRIA_ASSERT(builder.IsBufferDeclared(RG_RES_NAME(HistogramBuffer)));
					data.histogram = builder.ReadCopySrcBuffer(RG_RES_NAME(HistogramBuffer));
				},
				[=](HistogramCopyData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
				{
					cmd_list->CopyResource(histogram_copy->GetNative(), context.GetBuffer(data.histogram).GetNative());
				}, RGPassType::Compute, RGPassFlags::ForceNoCull);
		}

		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("Automatic Exposure", 0))
				{
					ImGui::SliderFloat("Min Luminance", &min_luminance, 0.0f, 1.0f);
					ImGui::SliderFloat("Max Luminance", &max_luminance, 0.3f, 20.0f);
					ImGui::SliderFloat("Adaption Speed", &adaption_speed, 0.01f, 5.0f);
					ImGui::SliderFloat("Exposure Compensation", &exposure_compensation, -5.0f, 5.0f);
					ImGui::SliderFloat("Low Percentile", &low_percentile, 0.0f, 0.49f);
					ImGui::SliderFloat("High Percentile", &high_percentile, 0.51f, 1.0f);
					ImGui::Checkbox("Histogram", &show_histogram);
					if (show_histogram)
					{
						ADRIA_ASSERT(histogram_copy->IsMapped());
						size_t histogram_size = histogram_copy->GetDesc().size / sizeof(int);
						int* hist_data = histogram_copy->GetMappedData<int>();
						int max_value = *std::max_element(hist_data, hist_data + histogram_size);
						auto converter = [](void* data, int idx)-> float
						{
							return static_cast<float>(*(((int*)data) + idx));
						};
						ImGui::PlotHistogram("Luminance Histogram", converter, hist_data, (int)histogram_size, 0, NULL, 0.0f, (float)max_value, ImVec2(0, 80));
					}
					ImGui::TreePop();
				}
			}
		);
	}

	void AutomaticExposurePass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

