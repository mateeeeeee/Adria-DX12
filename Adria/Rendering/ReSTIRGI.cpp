#include "ReSTIRGI.h"
#include "BlackboardData.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Editor/GUICommand.h"

namespace adria
{

	ReSTIRGI::ReSTIRGI(GfxDevice* gfx, uint32 width, uint32 height) : gfx(gfx), width(width), height(height)
	{
		CreateBuffers();
	}

	void ReSTIRGI::AddPasses(RenderGraph& rg)
	{
		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNode("ReSTIR GI"))
				{
					ImGui::Checkbox("Enable ReSTIR", &enable);
					ImGui::TreePop();
				}
			});
		if (!enable) return;

		AddInitialSamplingPass(rg);
		AddTemporalSamplingPass(rg);
		AddSpatialSamplingPass(rg);
	}

	void ReSTIRGI::AddInitialSamplingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		uint32 half_width = (width + 1) / 2;
		uint32 half_height = (height + 1) / 2;

		struct InitialSamplingPassData
		{
			RGTextureReadOnlyId depth_normal;
			RGTextureReadOnlyId prev_depth;
			RGTextureReadOnlyId irradiance_history;
			RGTextureReadWriteId irradiance;
			RGTextureReadWriteId ray_direction;
		};

		rg.AddPass<InitialSamplingPassData>("RESTIR GI Initial Sampling Pass",
			[=](InitialSamplingPassData& data, RenderGraphBuilder& builder)
			{
				data.depth_normal = builder.ReadTexture(RG_RES_NAME(DepthNormal));
				data.prev_depth = builder.ReadTexture(RG_RES_NAME(DepthHistory));

				RGTextureDesc irradiance_desc{};
				irradiance_desc.width = half_width;
				irradiance_desc.height = half_height;
				irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(ReSTIR_Irradiance), irradiance_desc);
				data.irradiance = builder.WriteTexture(RG_RES_NAME(ReSTIR_Irradiance));

				RGTextureDesc ray_direction_desc{};
				ray_direction_desc.width = half_width;
				ray_direction_desc.height = half_height;
				ray_direction_desc.format = GfxFormat::R32G32_UINT;
				builder.DeclareTexture(RG_RES_NAME(ReSTIR_RayDirection), irradiance_desc);
				data.ray_direction = builder.WriteTexture(RG_RES_NAME(ReSTIR_RayDirection));
			},
			[=](InitialSamplingPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				uint32 i = gfx->AllocateDescriptorsGPU(4).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.depth_normal));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadOnlyTexture(data.irradiance_history));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 2), ctx.GetReadWriteTexture(data.irradiance));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 3), ctx.GetReadWriteTexture(data.ray_direction));

				struct InitialSamplingPassParameters
				{
					uint32 depth_normal_idx;
					uint32 irradiance_history_idx;
					uint32 output_irradiance_idx;
					uint32 output_ray_direction_idx;
				} parameters = 
				{
					.depth_normal_idx = i,
					.irradiance_history_idx = i + 1,
					.output_irradiance_idx = i + 2,
					.output_ray_direction_idx = i + 3
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(width / 16.0f), 1);

			}, RGPassType::Compute);
	}

	void ReSTIRGI::AddTemporalSamplingPass(RenderGraph& rg)
	{

	}

	void ReSTIRGI::AddSpatialSamplingPass(RenderGraph& rg)
	{

	}

	void ReSTIRGI::CreateBuffers()
	{
		if (temporal_reservoir_buffers[0].reservoir == nullptr ||
			temporal_reservoir_buffers[0].reservoir->GetWidth() != width ||
			temporal_reservoir_buffers[0].reservoir->GetHeight() != height)
		{
			for (uint32 i = 0; i < 2; ++i)
			{
				GfxTextureDesc sample_radiance_desc{};
				sample_radiance_desc.width = width;
				sample_radiance_desc.height = height;
				sample_radiance_desc.mip_levels = 1;
				sample_radiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				sample_radiance_desc.bind_flags = GfxBindFlag::UnorderedAccess;
				sample_radiance_desc.initial_state = GfxResourceState::UnorderedAccess;
				temporal_reservoir_buffers[i].sample_radiance = gfx->CreateTexture(sample_radiance_desc);

				GfxTextureDesc ray_direction_desc{};
				ray_direction_desc.width = width;
				ray_direction_desc.height = height;
				ray_direction_desc.mip_levels = 1;
				ray_direction_desc.format = GfxFormat::R32_UINT;
				ray_direction_desc.bind_flags = GfxBindFlag::UnorderedAccess;
				ray_direction_desc.initial_state = GfxResourceState::UnorderedAccess;
				temporal_reservoir_buffers[i].ray_direction = gfx->CreateTexture(sample_radiance_desc);

				GfxTextureDesc depth_normal_desc{};
				depth_normal_desc.width = width;
				depth_normal_desc.height = height;
				depth_normal_desc.mip_levels = 1;
				depth_normal_desc.format = GfxFormat::R32G32_UINT;
				depth_normal_desc.bind_flags = GfxBindFlag::UnorderedAccess;
				depth_normal_desc.initial_state = GfxResourceState::UnorderedAccess;
				temporal_reservoir_buffers[i].depth_normal = gfx->CreateTexture(depth_normal_desc);

				GfxTextureDesc reservoir_desc{};
				reservoir_desc.width = width;
				reservoir_desc.height = height;
				reservoir_desc.mip_levels = 1;
				reservoir_desc.format = GfxFormat::R16G16_FLOAT;
				reservoir_desc.bind_flags = GfxBindFlag::UnorderedAccess;
				reservoir_desc.initial_state = GfxResourceState::UnorderedAccess;
				temporal_reservoir_buffers[i].reservoir = gfx->CreateTexture(reservoir_desc);
			}
		}
	}

}

