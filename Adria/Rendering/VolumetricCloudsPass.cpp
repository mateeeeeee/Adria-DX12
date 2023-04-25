#include "VolumetricCloudsPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "RenderGraph/RenderGraph.h"
#include "TextureManager.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Logging/Logger.h"
#include "Editor/GUICommand.h"

namespace adria
{

	VolumetricCloudsPass::VolumetricCloudsPass(uint32 w, uint32 h)
		: width{ w }, height{ h }
	{}

	void VolumetricCloudsPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		rg.ImportTexture(RG_RES_NAME(PreviousCloudsOutput), prev_clouds.get());

		struct VolumetricCloudsPassData
		{
			RGTextureReadOnlyId prev_output;
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
				data.output = builder.WriteTexture(RG_RES_NAME(CloudsOutput));
				data.prev_output = builder.ReadTexture(RG_RES_NAME(PreviousCloudsOutput), ReadAccess_NonPixelShader);
			},
			[=](VolumetricCloudsPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { g_TextureManager.GetSRV(cloud_textures[0]), 
												g_TextureManager.GetSRV(cloud_textures[1]),
												g_TextureManager.GetSRV(cloud_textures[2]), 
												context.GetReadWriteTexture(data.output), 
												context.GetReadOnlyTexture(data.prev_output) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
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
					float max_draw_distance;
				} constants =
				{
					.clouds_bottom_height = params.clouds_bottom_height,
					.clouds_top_height = params.clouds_top_height,
					.crispiness = params.crispiness,
					.curliness = params.curliness,
					.coverage = params.coverage,
					.cloud_type = params.cloud_type,
					.absorption = params.light_absorption,
					.density_factor = params.density_factor,
					.max_draw_distance = params.max_draw_distance
				};

				struct TextureIndices
				{
					uint32 weather_idx;
					uint32 cloud_idx;
					uint32 worley_idx;
					uint32 output_idx;
					uint32 prev_output_idx;
				} indices =
				{
					.weather_idx = i, .cloud_idx = i + 1, .worley_idx = i + 2,
					.output_idx = i + 3, .prev_output_idx = i + 4
				};

				GfxPipelineState* clouds_pso = PSOCache::Get(temporal_reprojection ? GfxPipelineStateID::Clouds_Reprojection : GfxPipelineStateID::Clouds);
				cmd_list->SetPipelineState(clouds_pso);
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, indices);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);

			}, RGPassType::Compute, RGPassFlags::None);

		if (temporal_reprojection)
		{
			struct CopyCloudsPassData
			{
				RGTextureCopySrcId copy_src;
				RGTextureCopyDstId copy_dst;
			};
			rg.AddPass<CopyCloudsPassData>("Clouds Copy Pass",
				[=](CopyCloudsPassData& data, RenderGraphBuilder& builder)
				{
					data.copy_dst = builder.WriteCopyDstTexture(RG_RES_NAME(PreviousCloudsOutput));
					data.copy_src = builder.ReadCopySrcTexture(RG_RES_NAME(CloudsOutput));
				},
				[=](CopyCloudsPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
				{
					GfxTexture const& src_texture = context.GetCopySrcTexture(data.copy_src);
					GfxTexture& dst_texture = context.GetCopyDstTexture(data.copy_dst);
					cmd_list->CopyTexture(dst_texture, src_texture);
				}, RGPassType::Copy, RGPassFlags::ForceNoCull);
		}

		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("Volumetric Clouds", 0))
				{
					ImGui::Checkbox("Temporal reprojection", &temporal_reprojection);
					ImGui::SliderFloat("Sun light absorption", &params.light_absorption, 0.0f, 0.015f);
					ImGui::SliderFloat("Clouds bottom height", &params.clouds_bottom_height, 1000.0f, 10000.0f);
					ImGui::SliderFloat("Clouds top height", &params.clouds_top_height, 10000.0f, 50000.0f);
					ImGui::SliderFloat("Density", &params.density_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Crispiness", &params.crispiness, 0.0f, 100.0f);
					ImGui::SliderFloat("Curliness", &params.curliness, 0.0f, 5.0f);
					ImGui::SliderFloat("Coverage", &params.coverage, 0.0f, 1.0f);
					ImGui::SliderFloat("Cloud Type", &params.cloud_type, 0.0f, 1.0f);
					ImGui::SliderFloat("Max Draw Distance", &params.max_draw_distance, 5000.0f, 15000.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);

		//AddGUI_Debug([&](void* descriptor_ptr)
		//	{
		//		if (ImGui::TreeNodeEx("Clouds Debug", ImGuiTreeNodeFlags_OpenOnDoubleClick))
		//		{
		//			GfxDescriptor gpu_descriptor = *static_cast<GfxDescriptor*>(descriptor_ptr);
		//			ImVec2 v_min = ImGui::GetWindowContentRegionMin();
		//			ImVec2 v_max = ImGui::GetWindowContentRegionMax();
		//			v_min.x += ImGui::GetWindowPos().x;
		//			v_min.y += ImGui::GetWindowPos().y;
		//			v_max.x += ImGui::GetWindowPos().x;
		//			v_max.y += ImGui::GetWindowPos().y;
		//			ImVec2 size(v_max.x - v_min.x, v_max.y - v_min.y);
		//
		//			GfxDevice* gfx = prev_clouds->GetParent();
		//			GfxDescriptor tex_srv = gfx->CreateTextureSRV(prev_clouds.get());
		//			gfx->CopyDescriptors(1, gpu_descriptor, tex_srv);
		//			gfx->FreeDescriptorCPU(tex_srv, GfxDescriptorHeapType::CBV_SRV_UAV);
		//			ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(gpu_descriptor).ptr, size);
		//			ImGui::TreePop();
		//			ImGui::Separator();
		//		}
		//	});
	}

	void VolumetricCloudsPass::AddCombinePass(RenderGraph& rendergraph, RGResourceName render_target)
	{
		struct CloudsCombinePassData
		{
			RGTextureReadOnlyId clouds_src;
		};

		rendergraph.AddPass<CloudsCombinePassData>("Clouds Combine Pass",
			[=](CloudsCombinePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(render_target, RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				data.clouds_src = builder.ReadTexture(RG_RES_NAME(BlurredCloudsOutput), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CloudsCombinePassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::CloudsCombine));
				GfxDescriptor dst = gfx->AllocateDescriptorsGPU();
				gfx->CopyDescriptors(1, dst, context.GetReadOnlyTexture(data.clouds_src));

				cmd_list->SetRootConstant(1, dst.GetIndex(), 0);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleStrip);
				cmd_list->Draw(4);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void VolumetricCloudsPass::OnResize(GfxDevice* gfx, uint32 w, uint32 h)
	{
		width = w, height = h;
		if (prev_clouds)
		{
			GfxTextureDesc clouds_output_desc = prev_clouds->GetDesc();
			clouds_output_desc.width = width;
			clouds_output_desc.height = height;
			prev_clouds = std::make_unique<GfxTexture>(gfx, clouds_output_desc);
		}
	}

	void VolumetricCloudsPass::OnSceneInitialized(GfxDevice* gfx)
	{
		GfxTextureDesc clouds_output_desc{};
		clouds_output_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
		clouds_output_desc.width = width;
		clouds_output_desc.height = height;
		clouds_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
		clouds_output_desc.bind_flags = GfxBindFlag::ShaderResource;
		clouds_output_desc.initial_state = GfxResourceState::NonPixelShaderResource;

		prev_clouds = std::make_unique<GfxTexture>(gfx, clouds_output_desc);

		cloud_textures.push_back(g_TextureManager.LoadTexture(L"Resources\\Textures\\clouds\\weather.dds"));
		cloud_textures.push_back(g_TextureManager.LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds"));
		cloud_textures.push_back(g_TextureManager.LoadTexture(L"Resources\\Textures\\clouds\\worley.dds"));
	}

}
