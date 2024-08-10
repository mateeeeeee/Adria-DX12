#include "ReSTIRGI.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Editor/GUICommand.h"

namespace adria
{

	ReSTIRGI::ReSTIRGI(GfxDevice* gfx, uint32 width, uint32 height) : gfx(gfx), width(width), height(height)
	{
		if (gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1))
		{
			GfxComputePipelineStateDesc compute_pso_desc{};
			compute_pso_desc.CS = CS_ReSTIRGI_InitialSampling;
			initial_sampling_pso = gfx->CreateComputePipelineState(compute_pso_desc);
		}
		CreateBuffers();
	}

	void ReSTIRGI::AddPasses(RenderGraph& rg)
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNode("ReSTIR GI"))
				{
					ImGui::Checkbox("Enable ReSTIR", &enable);
					static int current_resampling_mode = static_cast<int>(resampling_mode);
					if (ImGui::Combo("Resampling mode", &current_resampling_mode, "None\0Temporal\0Spatial\0TemporalAndSpatial\0", 4))
					{
						resampling_mode = static_cast<ResamplingMode>(current_resampling_mode);
					}
					ImGui::TreePop();
				}
			});
		if (!enable) return;

		AddInitialSamplingPass(rg);
		if (resampling_mode != ResamplingMode::None)
		{
			std::swap(temporal_reservoir_buffers[0], temporal_reservoir_buffers[1]);
			if (resampling_mode == ResamplingMode::Temporal || resampling_mode == ResamplingMode::TemporalAndSpatial) AddTemporalResamplingPass(rg);
			if (resampling_mode == ResamplingMode::Spatial  || resampling_mode == ResamplingMode::TemporalAndSpatial) AddSpatialResamplingPass(rg);
		}
	}

	void ReSTIRGI::AddInitialSamplingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		uint32 half_width = (width + 1) / 2;
		uint32 half_height = (height + 1) / 2;

		struct InitialSamplingPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId prev_depth;
			RGTextureReadOnlyId irradiance_history;
			RGTextureReadWriteId irradiance;
			RGTextureReadWriteId ray_direction;
		};

		rg.AddPass<InitialSamplingPassData>("RESTIR GI Initial Sampling Pass",
			[=](InitialSamplingPassData& data, RenderGraphBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.prev_depth = builder.ReadTexture(RG_NAME(DepthHistory));

				RGTextureDesc irradiance_desc{};
				irradiance_desc.width = half_width;
				irradiance_desc.height = half_height;
				irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(ReSTIR_Irradiance), irradiance_desc);
				data.irradiance = builder.WriteTexture(RG_NAME(ReSTIR_Irradiance));

				RGTextureDesc ray_direction_desc{};
				ray_direction_desc.width = half_width;
				ray_direction_desc.height = half_height;
				ray_direction_desc.format = GfxFormat::R32G32_UINT;
				builder.DeclareTexture(RG_NAME(ReSTIR_RayDirection), irradiance_desc);
				data.ray_direction = builder.WriteTexture(RG_NAME(ReSTIR_RayDirection));
			},
			[=](InitialSamplingPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				uint32 i = gfx->AllocateDescriptorsGPU(4).GetIndex();
				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadOnlyTexture(data.normal),
					ctx.GetReadOnlyTexture(data.irradiance_history),
					ctx.GetReadWriteTexture(data.irradiance),
					ctx.GetReadWriteTexture(data.ray_direction)
				};
				gfx->CopyDescriptors(gfx->GetDescriptorGPU(i), src_descriptors);

				struct InitialSamplingPassParameters
				{
					uint32 depth_idx;
					uint32 normal_idx;
					uint32 irradiance_history_idx;
					uint32 output_irradiance_idx;
					uint32 output_ray_direction_idx;
				} parameters = 
				{
					.depth_idx = i,
					.normal_idx = i + 1,
					.irradiance_history_idx = i + 2,
					.output_irradiance_idx = i + 3,
					.output_ray_direction_idx = i + 4
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(initial_sampling_pso.get());
				cmd_list->Dispatch(DivideAndRoundUp(half_width, 16), DivideAndRoundUp(half_height, 16), 1);
			}, RGPassType::Compute);
	}

	void ReSTIRGI::AddTemporalResamplingPass(RenderGraph& rg)
	{
		struct TemporalResamplingPassData
		{

		};

		rg.AddPass<TemporalResamplingPassData>("ReSTIR GI Temporal Resampling Pass", 
			[=](TemporalResamplingPassData& data, RGBuilder& builder)
			{
				
			},
			[=](TemporalResamplingPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{
				
			}, RGPassType::Compute);
	}

	void ReSTIRGI::AddSpatialResamplingPass(RenderGraph& rg)
	{
		struct SpatialResamplingPassData
		{

		};

		rg.AddPass<SpatialResamplingPassData>("ReSTIR GI Spatial Resampling Pass",
			[=](SpatialResamplingPassData& data, RGBuilder& builder)
			{

			},
			[=](SpatialResamplingPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{

			}, RGPassType::Compute);
	}

	void ReSTIRGI::CreateBuffers()
	{
		if (temporal_reservoir_buffers[0].reservoir == nullptr ||
			temporal_reservoir_buffers[0].reservoir->GetWidth() != width ||
			temporal_reservoir_buffers[0].reservoir->GetHeight() != height)
		{
			for (uint32 i = 0; i < ARRAYSIZE(temporal_reservoir_buffers); ++i)
			{
				GfxTextureDesc sample_radiance_desc{};
				sample_radiance_desc.width = width;
				sample_radiance_desc.height = height;
				sample_radiance_desc.mip_levels = 1;
				sample_radiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				sample_radiance_desc.bind_flags = GfxBindFlag::UnorderedAccess;
				sample_radiance_desc.initial_state = GfxResourceState::ComputeUAV;
				temporal_reservoir_buffers[i].sample_radiance = gfx->CreateTexture(sample_radiance_desc);

				GfxTextureDesc ray_direction_desc{};
				ray_direction_desc.width = width;
				ray_direction_desc.height = height;
				ray_direction_desc.mip_levels = 1;
				ray_direction_desc.format = GfxFormat::R32_UINT;
				ray_direction_desc.bind_flags = GfxBindFlag::UnorderedAccess;
				ray_direction_desc.initial_state = GfxResourceState::ComputeUAV;
				temporal_reservoir_buffers[i].ray_direction = gfx->CreateTexture(sample_radiance_desc);

				GfxTextureDesc reservoir_desc{};
				reservoir_desc.width = width;
				reservoir_desc.height = height;
				reservoir_desc.mip_levels = 1;
				reservoir_desc.format = GfxFormat::R16G16_FLOAT;
				reservoir_desc.bind_flags = GfxBindFlag::UnorderedAccess;
				reservoir_desc.initial_state = GfxResourceState::ComputeUAV;
				temporal_reservoir_buffers[i].reservoir = gfx->CreateTexture(reservoir_desc);
			}
		}
	}

}

