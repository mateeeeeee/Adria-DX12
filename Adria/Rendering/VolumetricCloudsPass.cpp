#include "VolumetricCloudsPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/RingGPUDescriptorAllocator.h"
#include "../Graphics/LinearDynamicAllocator.h"
#include "../Logging/Logger.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	VolumetricCloudsPass::VolumetricCloudsPass(uint32 w, uint32 h)
		: width{ w }, height{ h }
	{}

	void VolumetricCloudsPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		struct VolumetricCloudsPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};
		rg.AddPass<VolumetricCloudsPassData>("Volumetric Clouds Pass",
			[=](VolumetricCloudsPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc clouds_output_desc{};
				clouds_output_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				clouds_output_desc.width = width;
				clouds_output_desc.height = height;
				clouds_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(CloudsOutput), clouds_output_desc);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				data.output = builder.WriteTexture(RG_RES_NAME(CloudsOutput));
			},
			[=](VolumetricCloudsPassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(5);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { TextureManager::Get().GetSRV(cloud_textures[0]),  TextureManager::Get().GetSRV(cloud_textures[1]),
															 TextureManager::Get().GetSRV(cloud_textures[2]), context.GetReadOnlyTexture(data.depth),
															context.GetReadWriteTexture(data.output) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(i) };
				uint32 src_range_sizes[] = { 1, 1, 1, 1, 1 };
				uint32 dst_range_sizes[] = { 5 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct CloudsConstants
				{
					float clouds_bottom_height;
					float clouds_top_height;
					float crispiness;
					float curliness;
					float coverage;
					float cloud_type;
					float absorption;
					float density_factor;
				} constants =
				{
					.clouds_bottom_height = params.clouds_bottom_height,
					.clouds_top_height = params.clouds_top_height,
					.crispiness = params.crispiness,
					.curliness = params.curliness,
					.coverage = params.coverage,
					.cloud_type = params.cloud_type,
					.absorption = params.light_absorption,
					.density_factor = params.density_factor
				};

				struct TextureIndices
				{
					uint32 weather_idx;
					uint32 cloud_idx;
					uint32 worley_idx;
					uint32 depth_idx;
					uint32 output_idx;
				} indices =
				{
					.weather_idx = i, .cloud_idx = i + 1, .worley_idx = i + 2,
					.depth_idx = i + 3,.output_idx = i + 4
				};

				DynamicAllocation allocation = dynamic_allocator->Allocate(GetCBufferSize<TextureIndices>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				allocation.Update(indices);

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Clouds));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 8, &constants, 0);
				cmd_list->SetComputeRootConstantBufferView(2, allocation.gpu_address);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);

			}, RGPassType::Compute, RGPassFlags::None);
	
		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("Volumetric Clouds", 0))
				{
					ImGui::SliderFloat("Sun light absorption", &params.light_absorption, 0.0f, 0.015f);
					ImGui::SliderFloat("Clouds bottom height", &params.clouds_bottom_height, 1000.0f, 10000.0f);
					ImGui::SliderFloat("Clouds top height", &params.clouds_top_height, 10000.0f, 50000.0f);
					ImGui::SliderFloat("Density", &params.density_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Crispiness", &params.crispiness, 0.0f, 100.0f);
					ImGui::SliderFloat("Curliness", &params.curliness, 0.0f, 5.0f);
					ImGui::SliderFloat("Coverage", &params.coverage, 0.0f, 1.0f);
					ImGui::SliderFloat("Cloud Type", &params.cloud_type, 0.0f, 1.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

	void VolumetricCloudsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void VolumetricCloudsPass::OnSceneInitialized(GfxDevice* gfx)
	{
		cloud_textures.push_back(TextureManager::Get().LoadTexture(L"Resources\\Textures\\clouds\\weather.dds"));
		cloud_textures.push_back(TextureManager::Get().LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds"));
		cloud_textures.push_back(TextureManager::Get().LoadTexture(L"Resources\\Textures\\clouds\\worley.dds"));
	}

}
