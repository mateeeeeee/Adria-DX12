#include "VolumetricCloudsPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "TextureManager.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../Graphics/GfxLinearDynamicAllocator.h"
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
			[=](VolumetricCloudsPassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				
				GfxDescriptor src_handles[] = { TextureManager::Get().GetSRV(cloud_textures[0]),  TextureManager::Get().GetSRV(cloud_textures[1]),
															 TextureManager::Get().GetSRV(cloud_textures[2]), context.GetReadOnlyTexture(data.depth),
															context.GetReadWriteTexture(data.output) };
				GfxDescriptor dst_handle = descriptor_allocator->Allocate(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);

				uint32 i = dst_handle.GetIndex();
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

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Clouds));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, indices);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);

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
