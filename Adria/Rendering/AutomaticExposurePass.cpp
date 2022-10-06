#include "AutomaticExposurePass.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/Texture.h"

namespace adria
{

	AutomaticExposurePass::AutomaticExposurePass(uint32 w, uint32 h)
		: width(w), height(h)
	{
	}

	void AutomaticExposurePass::CreateResources(GraphicsDevice* gfx)
	{
		TextureDesc desc{};
		desc.width = 1;
		desc.height = 1;
		desc.mip_levels = 1;
		desc.bind_flags = EBindFlag::UnorderedAccess;
		desc.misc_flags = ETextureMiscFlag::None;
		desc.initial_state = EResourceState::UnorderedAccess;
		desc.format = EFormat::R16_FLOAT;

		previous_ev100 = std::make_unique<Texture>(gfx, desc);
	}

	void AutomaticExposurePass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		struct BuildHistogramData
		{
			RGTextureReadOnlyId scene_texture;
			RGBufferReadWriteId histogram_buffer;
		};
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rg.AddPass<BuildHistogramData>("Build Histogram Pass",
			[=](BuildHistogramData& data, RenderGraphBuilder& builder)
			{
				data.scene_texture = builder.ReadTexture(input);

				RGBufferDesc desc{};
				desc.stride = sizeof(uint32);
				desc.size = desc.stride * 256;
				desc.format = EFormat::R32_FLOAT;
				desc.misc_flags = EBufferMiscFlag::BufferRaw;
				desc.resource_usage = EResourceUsage::Default;
				builder.DeclareBuffer(RG_RES_NAME(HistogramBuffer), desc);
				data.histogram_buffer = builder.WriteBuffer(RG_RES_NAME(HistogramBuffer));
			},
			[=](BuildHistogramData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				DescriptorCPU buffer_uav_cpu = context.GetReadWriteBuffer(data.histogram_buffer);
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				
				DescriptorHandle buffer_gpu = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, buffer_gpu, buffer_uav_cpu,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				Buffer const& histogram_buffer = context.GetBuffer(data.histogram_buffer.GetResourceId());

				uint32 clear_value[4] = { 0, 0, 0, 0 };
				cmd_list->ClearUnorderedAccessViewUint(buffer_gpu, buffer_uav_cpu, histogram_buffer.GetNative(), clear_value, 0, nullptr);

				D3D12_RESOURCE_BARRIER buffer_uav_barrier{};
				buffer_uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				buffer_uav_barrier.UAV.pResource = histogram_buffer.GetNative();
				cmd_list->ResourceBarrier(1, &buffer_uav_barrier);

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::BuildHistogram));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::BuildHistogram)); 
				uint32 half_width = (width + 1) / 2;
				uint32 half_height = (height + 1) / 2;

				struct BuildHistogramConstants
				{
					uint32  width;
					uint32  height;
					float32 rcp_width;
					float32 rcp_height;
					float32 min_luminance;
					float32 max_luminance;
				} constants = {	.width = half_width, .height = half_height, 
								.rcp_width = 1.0f / half_width, .rcp_height = 1.0f / half_height,
								.min_luminance = min_luminance, .max_luminance = max_luminance};

				DynamicAllocation cb_alloc = dynamic_allocator->Allocate(sizeof(BuildHistogramConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				cb_alloc.Update(constants);

				descriptor_index = descriptor_allocator->Allocate();
				DescriptorHandle scene_srv = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, scene_srv, context.GetReadOnlyTexture(data.scene_texture),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetComputeRootDescriptorTable(0, buffer_gpu);
				cmd_list->SetComputeRootDescriptorTable(1, scene_srv);
				cmd_list->SetComputeRootConstantBufferView(2, cb_alloc.gpu_address);

				auto DivideRoudingUp = [](uint32 a, uint32 b)
				{
					return (a + b - 1) / b;
				};
				cmd_list->Dispatch(DivideRoudingUp(half_width, 16), DivideRoudingUp(half_height, 16), 1);
			}, ERGPassType::Compute, ERGPassFlags::ForceNoCull);

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
				desc.format = EFormat::R16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(AverageLuminance), desc);
				data.avg_luminance = builder.WriteTexture(RG_RES_NAME(AverageLuminance));
			},
			[=](HistogramReductionData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::HistogramReduction));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::HistogramReduction));

				struct HistogramReductionConstants 
				{
					float32 min_luminance;
					float32 max_luminance;
					float32 low_percentile;
					float32 high_percentile;
				} constants = { .min_luminance = min_luminance, .max_luminance = max_luminance,
								.low_percentile = low_percentile, .high_percentile = high_percentile };
				cmd_list->SetComputeRoot32BitConstants(0, 4, &constants, 0);
				
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
				DescriptorHandle buffer_srv = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, buffer_srv, context.GetReadOnlyBuffer(data.histogram_buffer),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, buffer_srv);
				DescriptorHandle avgluminance_uav = descriptor_allocator->GetHandle(descriptor_index + 1);
				device->CopyDescriptorsSimple(1, avgluminance_uav, context.GetReadWriteTexture(data.avg_luminance),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, avgluminance_uav);

				cmd_list->Dispatch(1, 1, 1);
			}, ERGPassType::Compute, ERGPassFlags::ForceNoCull);

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
				desc.format = EFormat::R16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(Exposure), desc);
				data.exposure = builder.WriteTexture(RG_RES_NAME(Exposure));
			},
			[=](ExposureData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();
				//if (invalid_history)
				//{
				//	DescriptorCPU cpu_descriptor = previous_ev100->GetSubresource_UAV();
				//	OffsetType descriptor_index = descriptor_allocator->Allocate();
				//	DescriptorHandle gpu_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				//	device->CopyDescriptorsSimple(1, gpu_descriptor, cpu_descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				//
				//	float32 clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				//	cmd_list->ClearUnorderedAccessViewFloat(gpu_descriptor, cpu_descriptor, previous_ev100->GetNative(), clear_value, 0, nullptr);
				//	invalid_history = false;
				//}


			}, ERGPassType::Compute, ERGPassFlags::ForceNoCull);

		
	}

	void AutomaticExposurePass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

