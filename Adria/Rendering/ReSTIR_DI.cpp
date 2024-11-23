#include "ReSTIR_DI.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Editor/GUICommand.h"

namespace adria
{
	struct ReSTIR_DI_ReservoirSample
	{
		Vector3 sample_position;
		Vector3 sample_normal;
		Vector3 sample_radiance;
	};

	struct ReSTIR_DI_Reservoir
	{
		ReSTIR_DI_ReservoirSample sample;
		Float target_function;
		Float num_samples;
		Float weight_sum;
	};


	ReSTIR_DI::ReSTIR_DI(GfxDevice* gfx, Uint32 width, Uint32 height) : gfx(gfx), width(width), height(height)
	{
		if (!gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1))
		{
			return;
		}
		
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_ReSTIRGI_InitialSampling;
		initial_sampling_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_ReSTIRGI_TemporalResampling;
		temporal_resampling_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_ReSTIRGI_SpatialResampling;
		spatial_resampling_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		CreateBuffers();
		supported = true;
	}

	void ReSTIR_DI::AddPasses(RenderGraph& rg)
	{
		if (!supported)
		{
			return;
		}
		QueueGUI([&]()
			{
				if (ImGui::TreeNode("ReSTIR GI"))
				{
					ImGui::Checkbox("Enable ReSTIR", &enable);
					static Sint current_resampling_mode = static_cast<Sint>(resampling_mode);
					if (ImGui::Combo("Resampling mode", &current_resampling_mode, "None\0Temporal\0Spatial\0TemporalAndSpatial\0", 4))
					{
						resampling_mode = static_cast<ResamplingMode>(current_resampling_mode);
					}
					ImGui::TreePop();
				}
			});
		if (!enable)
		{
			return;
		}

		AddInitialSamplingPass(rg);

		if (resampling_mode != ResamplingMode::None)
		{
			if (resampling_mode == ResamplingMode::Temporal || resampling_mode == ResamplingMode::TemporalAndSpatial) AddTemporalResamplingPass(rg);
			if (resampling_mode == ResamplingMode::Spatial  || resampling_mode == ResamplingMode::TemporalAndSpatial) AddSpatialResamplingPass(rg);
		}
	}

	void ReSTIR_DI::AddInitialSamplingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

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
				data.irradiance_history = builder.ReadTexture(RG_NAME(ReSTIR_IrradianceHistory));

				RGTextureDesc irradiance_desc{};
				irradiance_desc.width = width;
				irradiance_desc.height = height;
				irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(ReSTIR_Irradiance), irradiance_desc);
				data.irradiance = builder.WriteTexture(RG_NAME(ReSTIR_Irradiance));

				RGTextureDesc ray_direction_desc{};
				ray_direction_desc.width = width;
				ray_direction_desc.height = height;
				ray_direction_desc.format = GfxFormat::R32_UINT;
				builder.DeclareTexture(RG_NAME(ReSTIR_RayDirection), irradiance_desc);
				data.ray_direction = builder.WriteTexture(RG_NAME(ReSTIR_RayDirection));
			},
			[=](InitialSamplingPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadOnlyTexture(data.normal),
					ctx.GetReadOnlyTexture(data.irradiance_history),
					ctx.GetReadWriteTexture(data.irradiance),
					ctx.GetReadWriteTexture(data.ray_direction)
				};
				Uint32 i = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors)).GetIndex();
				gfx->CopyDescriptors(gfx->GetDescriptorGPU(i), src_descriptors);

				struct InitialSamplingPassParameters
				{
					Uint32 depth_idx;
					Uint32 normal_idx;
					Uint32 irradiance_history_idx;
					Uint32 output_irradiance_idx;
					Uint32 output_ray_direction_idx;
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
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void ReSTIR_DI::AddTemporalResamplingPass(RenderGraph& rg)
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

	void ReSTIR_DI::AddSpatialResamplingPass(RenderGraph& rg)
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

	void ReSTIR_DI::CreateBuffers()
	{
		if (staging_reservoir_buffer == nullptr || final_reservoir_buffer == nullptr)
		{
			GfxBufferDesc reservoir_buffer_desc = StructuredBufferDesc<ReSTIR_DI_ReservoirSample>(width * height, true, false);
			staging_reservoir_buffer = gfx->CreateBuffer(reservoir_buffer_desc);
			final_reservoir_buffer = gfx->CreateBuffer(reservoir_buffer_desc);
		}
	}

}

