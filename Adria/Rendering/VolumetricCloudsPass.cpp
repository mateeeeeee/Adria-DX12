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

using namespace DirectX;

namespace adria
{

	VolumetricCloudsPass::VolumetricCloudsPass(uint32 w, uint32 h)
		: width{ w }, height{ h }
	{}

	void VolumetricCloudsPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		rg.ImportTexture(RG_RES_NAME(PreviousCloudsOutput), prev_clouds.get());

		static bool first_frame = true;
		if (first_frame || should_generate_textures)
		{
			first_frame = false;
			if (should_generate_textures)
			{
				CreateCloudTextures();
				should_generate_textures = false;
			}
			rg.ImportTexture(RG_RES_NAME(CloudShape), cloud_shape_noise.get());
			rg.ImportTexture(RG_RES_NAME(CloudDetail), cloud_detail_noise.get()); 
			rg.ImportTexture(RG_RES_NAME(CloudType), cloud_type.get()); 

			struct CloudNoiseConstants
			{
				float resolution_inv;
				uint32 frequency;
				uint32 output_idx;
			};

			for (uint32 i = 0; i < cloud_shape_noise->GetDesc().mip_levels; ++i)
			{
				struct CloudShapePassData
				{
					RGTextureReadWriteId shape;
				};

				rg.AddPass<CloudShapePassData>("Compute Cloud Shape",
					[=](CloudShapePassData& data, RenderGraphBuilder& builder)
					{
						data.shape = builder.WriteTexture(RG_RES_NAME(CloudShape), i, 1);
					},
					[=](CloudShapePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
					{
						GfxDevice* gfx = cmd_list->GetDevice();
						uint32 resolution = cloud_shape_noise->GetDesc().width >> i;

						GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(1);
						GfxDescriptor src_handles[] = { ctx.GetReadWriteTexture(data.shape) };
						gfx->CopyDescriptors(dst_handle, src_handles);
						uint32 j = dst_handle.GetIndex();

						CloudNoiseConstants constants
						{
							.resolution_inv = 1.0f / resolution,
							.frequency = (uint32)params.shape_noise_frequency,
							.output_idx = j
						};
						cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::CloudShape));
						cmd_list->SetRootConstants(1, constants);
						uint32 dispatch = (uint32)std::ceil(resolution * 1.0f / 8);
						cmd_list->Dispatch(dispatch, dispatch, dispatch);
					}, RGPassType::Compute, RGPassFlags::None);
			}

			for (uint32 i = 0; i < cloud_detail_noise->GetDesc().mip_levels; ++i)
			{
				struct CloudShapePassData
				{
					RGTextureReadWriteId detail;
				};

				rg.AddPass<CloudShapePassData>("Compute Cloud Detail",
					[=](CloudShapePassData& data, RenderGraphBuilder& builder)
					{
						data.detail = builder.WriteTexture(RG_RES_NAME(CloudDetail), i, 1);
					},
					[=](CloudShapePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
					{
						GfxDevice* gfx = cmd_list->GetDevice();
						uint32 resolution = cloud_detail_noise->GetDesc().width >> i;

						GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(1);
						GfxDescriptor src_handles[] = { ctx.GetReadWriteTexture(data.detail) };
						gfx->CopyDescriptors(dst_handle, src_handles);
						uint32 j = dst_handle.GetIndex();

						CloudNoiseConstants constants
						{
							.resolution_inv = 1.0f / resolution,
							.frequency = (uint32)params.detail_noise_frequency,
							.output_idx = j
						};
						cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::CloudDetail));
						cmd_list->SetRootConstants(1, constants);
						uint32 dispatch = (uint32)std::ceil(resolution * 1.0f / 8);
						cmd_list->Dispatch(dispatch, dispatch, dispatch);
					}, RGPassType::Compute, RGPassFlags::None);
			}

			struct CloudTypePassData
			{
				RGTextureReadWriteId type;
			};

			rg.AddPass<CloudTypePassData>("Compute Cloud Type",
				[=](CloudTypePassData& data, RenderGraphBuilder& builder)
				{
					data.type = builder.WriteTexture(RG_RES_NAME(CloudType));
				},
				[=](CloudTypePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();
					uint32 resolution = cloud_type->GetDesc().width;

					GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(1);
					GfxDescriptor src_handles[] = { ctx.GetReadWriteTexture(data.type) };
					gfx->CopyDescriptors(dst_handle, src_handles);
					uint32 j = dst_handle.GetIndex();

					CloudNoiseConstants constants
					{
						.resolution_inv = 1.0f / resolution,
						.output_idx = j
					};
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::CloudType));
					cmd_list->SetRootConstants(1, constants);
					uint32 dispatch = (uint32)std::ceil(resolution * 1.0f / 8);
					cmd_list->Dispatch(dispatch, dispatch, dispatch);
				}, RGPassType::Compute, RGPassFlags::None);
		}
		else
		{
			rg.ImportTexture(RG_RES_NAME(CloudShape), cloud_shape_noise.get());
			rg.ImportTexture(RG_RES_NAME(CloudDetail), cloud_detail_noise.get());
			rg.ImportTexture(RG_RES_NAME(CloudType), cloud_type.get());
		}

		struct VolumetricCloudsPassData
		{
			RGTextureReadOnlyId type;
			RGTextureReadOnlyId shape;
			RGTextureReadOnlyId detail;
			RGTextureReadOnlyId prev_output;
			RGTextureReadWriteId output;
		};
		rg.AddPass<VolumetricCloudsPassData>("Volumetric Clouds Pass",
			[=](VolumetricCloudsPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc clouds_output_desc{};
				clouds_output_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				clouds_output_desc.width = width >> resolution;
				clouds_output_desc.height = height >> resolution;
				clouds_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(CloudsOutput), clouds_output_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(CloudsOutput));
				data.type = builder.ReadTexture(RG_RES_NAME(CloudType), ReadAccess_NonPixelShader);
				data.shape = builder.ReadTexture(RG_RES_NAME(CloudShape), ReadAccess_NonPixelShader);
				data.detail = builder.ReadTexture(RG_RES_NAME(CloudDetail), ReadAccess_NonPixelShader);
				data.prev_output = builder.ReadTexture(RG_RES_NAME(PreviousCloudsOutput), ReadAccess_NonPixelShader);
			},
			[=](VolumetricCloudsPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.type),
												context.GetReadOnlyTexture(data.shape),
												context.GetReadOnlyTexture(data.detail),
												context.GetReadWriteTexture(data.output),
												context.GetReadOnlyTexture(data.prev_output) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);

				uint32 i = dst_handle.GetIndex();

				float noise_scale = 0.00001f + params.shape_noise_scale * 0.0004f;
				struct CloudsConstants
				{
					uint32      type_idx;
					uint32      shape_idx;
					uint32      detail_idx;
					uint32      output_idx;

					uint32      prev_output_idx;
					float		cloud_type;
					float 	    cloud_min_height;
					float 	    cloud_max_height;

					float 	    shape_noise_scale;
					float 	    detail_noise_scale;
					float 	    detail_noise_modifier;
					float       global_density;

					float 	    cloud_coverage;
					XMFLOAT3    cloud_base_color;
					XMFLOAT3    cloud_top_color;
					int	        max_num_steps;

					XMFLOAT3    planet_center;
					float 	    planet_radius;

					float 	    light_step_length;
					float 	    light_cone_radius;
					float 	    precipitation;
					float 	    ambient_light_factor;

					float 	    sun_light_factor;
					float 	    henyey_greenstein_g_forward;
					float 	    henyey_greenstein_g_backward;
					uint32      resolution_factor;
				} constants =
				{
					.type_idx = i + 0,
					.shape_idx = i + 1,
					.detail_idx = i + 2,
					.output_idx = i + 3,

					.prev_output_idx = i + 4,
					.cloud_type = params.cloud_type,
					.cloud_min_height = params.cloud_min_height,
					.cloud_max_height = params.cloud_max_height,

					.shape_noise_scale = noise_scale,
					.detail_noise_scale = params.detail_noise_scale * noise_scale,
					.detail_noise_modifier = params.detail_noise_modifier,
					.global_density = params.global_density,

					.cloud_coverage = params.cloud_coverage,
					.cloud_base_color = XMFLOAT3(params.cloud_base_color),
					.cloud_top_color = XMFLOAT3(params.cloud_top_color),
					.max_num_steps = params.max_num_steps,

					.planet_center = XMFLOAT3(0.0f, -params.planet_radius, 0.0f),
					.planet_radius = params.planet_radius,
					
					.light_step_length = params.light_step_length,
					.light_cone_radius = params.light_cone_radius,
					.precipitation = params.precipitation * 0.01f,
					.ambient_light_factor = params.ambient_light_factor,

					.sun_light_factor = params.sun_light_factor,
					.henyey_greenstein_g_forward = params.henyey_greenstein_g_forward,
					.henyey_greenstein_g_backward = params.henyey_greenstein_g_backward,
					.resolution_factor = (uint32)resolution
				};

				GfxPipelineState* clouds_pso = PSOCache::Get(temporal_reprojection ? GfxPipelineStateID::Clouds_Reprojection : GfxPipelineStateID::Clouds);
				cmd_list->SetPipelineState(clouds_pso);
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch((uint32)std::ceil((width >> resolution) / 16.0f), (uint32)std::ceil((height >> resolution) / 16.0f), 1);

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

		AddCombinePass(rg, RG_RES_NAME(PostprocessMain));

		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("Volumetric Clouds", 0))
				{
					ImGui::Checkbox("Temporal reprojection", &temporal_reprojection);
					should_generate_textures |= ImGui::SliderInt("Shape Noise Frequency", &params.shape_noise_frequency, 1, 10);
					should_generate_textures |= ImGui::SliderInt("Shape Noise Resolution", &params.shape_noise_resolution, 32, 256);
					should_generate_textures |= ImGui::SliderInt("Detail Noise Frequency", &params.detail_noise_frequency, 1, 10);
					should_generate_textures |= ImGui::SliderInt("Detail Noise Resolution", &params.detail_noise_resolution, 8, 64);

					ImGui::SliderFloat("Cloud Type", &params.cloud_type, 0.0f, 1.0f);
					ImGui::SliderFloat("Global Density", &params.global_density, 0.0f, 1.0f);
					ImGui::SliderFloat("Cloud Min Height", &params.cloud_min_height, 1000.0f, 20000.0f);
					ImGui::SliderFloat("Cloud Max Height", &params.cloud_max_height, params.cloud_min_height, 20000.0f);
					ImGui::SliderFloat("Shape Noise Scale", &params.shape_noise_scale, 0.1f, 1.0f);
					ImGui::SliderFloat("Detail Noise Scale", &params.detail_noise_scale, 0.0f, 100.0f);
					ImGui::SliderFloat("Detail Noise Modifier", &params.detail_noise_modifier, 0.0f, 1.0f);
					ImGui::SliderFloat("Cloud Coverage", &params.cloud_coverage, 0.0f, 1.0f);
					ImGui::SliderFloat("Precipitation", &params.precipitation, 1.0f, 2.5f);
					ImGui::SliderFloat("Ambient Factor", &params.ambient_light_factor, 0.01f, 10.0f);
					ImGui::SliderFloat("Sun Factor", &params.sun_light_factor, 0.1f, 10.0f);
					ImGui::InputFloat("Planet Radius", &params.planet_radius);
					ImGui::SliderInt("Max Num Steps", &params.max_num_steps, 16, 256);

					static int _resolution = 0;
					static const char* res_types[] = { "Full", "Half", "Quarter" };
					const char* res_combo_label = res_types[_resolution];
					if (ImGui::BeginCombo("Cloud Resolution", res_combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(res_types); n++)
						{
							const bool is_selected = (_resolution == n);
							if (ImGui::Selectable(res_types[n], is_selected)) _resolution = (CloudResolution)n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (resolution != _resolution)
					{
						resolution = (CloudResolution)_resolution;
						OnResize(nullptr, width, height);
					}
					
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
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
				data.clouds_src = builder.ReadTexture(RG_RES_NAME(CloudsOutput), ReadAccess_PixelShader);
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
			if (!gfx) gfx = cloud_shape_noise->GetParent();

			GfxTextureDesc clouds_output_desc = prev_clouds->GetDesc();
			clouds_output_desc.width = width >> resolution;
			clouds_output_desc.height = height >> resolution;
			prev_clouds = std::make_unique<GfxTexture>(gfx, clouds_output_desc);
		}
	}

	void VolumetricCloudsPass::OnSceneInitialized(GfxDevice* gfx)
	{
		GfxTextureDesc clouds_output_desc{};
		clouds_output_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
		clouds_output_desc.width = width >> resolution;
		clouds_output_desc.height = height >> resolution;
		clouds_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
		clouds_output_desc.bind_flags = GfxBindFlag::ShaderResource;
		clouds_output_desc.initial_state = GfxResourceState::NonPixelShaderResource;

		prev_clouds = std::make_unique<GfxTexture>(gfx, clouds_output_desc);
		CreateCloudTextures(gfx);
	}

	void VolumetricCloudsPass::CreateCloudTextures(GfxDevice* gfx)
	{
		if (!gfx) gfx = cloud_shape_noise->GetParent();

		GfxTextureDesc cloud_shape_noise_desc{};
		cloud_shape_noise_desc.type = GfxTextureType_3D;
		cloud_shape_noise_desc.width = params.shape_noise_resolution;
		cloud_shape_noise_desc.height = params.shape_noise_resolution;
		cloud_shape_noise_desc.depth = params.shape_noise_resolution;
		cloud_shape_noise_desc.mip_levels = 4;
		cloud_shape_noise_desc.format = GfxFormat::R8G8B8A8_UNORM;
		cloud_shape_noise_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;

		cloud_shape_noise = std::make_unique<GfxTexture>(gfx, cloud_shape_noise_desc);

		GfxTextureDesc cloud_detail_noise_desc{};
		cloud_detail_noise_desc.type = GfxTextureType_3D;
		cloud_detail_noise_desc.width = params.shape_noise_resolution;
		cloud_detail_noise_desc.height = params.shape_noise_resolution;
		cloud_detail_noise_desc.depth = params.shape_noise_resolution;
		cloud_detail_noise_desc.mip_levels = 4;
		cloud_detail_noise_desc.format = GfxFormat::R8G8B8A8_UNORM;
		cloud_detail_noise_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;

		cloud_detail_noise = std::make_unique<GfxTexture>(gfx, cloud_detail_noise_desc);

		GfxTextureDesc cloud_type_desc{};
		cloud_type_desc.type = GfxTextureType_2D;
		cloud_type_desc.width = 128;
		cloud_type_desc.height = 128;
		cloud_type_desc.mip_levels = 1;
		cloud_type_desc.format = GfxFormat::R8_UNORM;
		cloud_type_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;

		cloud_type = std::make_unique<GfxTexture>(gfx, cloud_type_desc);
	}

}

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