#include "VolumetricCloudsPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../Logging/Logger.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	VolumetricCloudsPass::VolumetricCloudsPass(TextureManager& texture_manager, uint32 w, uint32 h)
		: texture_manager{ texture_manager }, width{ w }, height{ h }
	{}

	void VolumetricCloudsPass::AddPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct VolumetricCloudsPassData
		{
			RGTextureReadOnlyId depth;
		};
		rg.AddPass<VolumetricCloudsPassData>("Volumetric Clouds Pass",
			[=](VolumetricCloudsPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc clouds_output_desc{};
				clouds_output_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				clouds_output_desc.width = width;
				clouds_output_desc.height = height;
				clouds_output_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(CloudsOutput), clouds_output_desc);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(CloudsOutput), ERGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](VolumetricCloudsPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Clouds));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Clouds));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.weather_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(4);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { texture_manager.CpuDescriptorHandle(cloud_textures[0]),  texture_manager.CpuDescriptorHandle(cloud_textures[1]),
															 texture_manager.CpuDescriptorHandle(cloud_textures[2]), context.GetReadOnlyTexture(data.depth) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1, 1, 1 };
				uint32 dst_range_sizes[] = { 4 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	
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
					ImGui::SliderFloat("Wind speed factor", &params.wind_speed, 0.0f, 100.0f);
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

	void VolumetricCloudsPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\weather.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\worley.dds"));
	}

}
