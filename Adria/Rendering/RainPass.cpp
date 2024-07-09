#include "RainPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "TextureManager.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxDevice.h"
#include "Editor/GUICommand.h"
#include "Core/Paths.h"
#include "Utilities/Random.h"

namespace adria
{
	struct RainData
	{
		Vector3 position;
		Vector3 velocity;
		float   state;
	};
	
	RainPass::RainPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), rain_blocker_map_pass(reg, gfx, w, h)
	{
	}

	void RainPass::Update(float dt)
	{
		if (!pause_simulation)
		{
			rain_total_time += dt;
		}
	}

	void RainPass::AddBlockerPass(RenderGraph& rg)
	{
		rain_blocker_map_pass.AddPass(rg);
	}

	void RainPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.ImportBuffer(RG_RES_NAME(RainDataBuffer), rain_data_buffer.get());

		if (!pause_simulation)
		{
			struct RainSimulationPassData
			{
				RGBufferReadWriteId rain_data_buffer;
			};
			rg.AddPass<RainSimulationPassData>("Rain Simulation Pass",
				[=](RainSimulationPassData& data, RenderGraphBuilder& builder)
				{
					data.rain_data_buffer = builder.WriteBuffer(RG_RES_NAME(RainDataBuffer));
				},
				[=](RainSimulationPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();

					GfxDescriptor src_handle = context.GetReadWriteBuffer(data.rain_data_buffer);
					GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_handle, src_handle);

					uint32 i = dst_handle.GetIndex();
					struct RainSimulationConstants
					{
						uint32   rain_data_idx;
						uint32   depth_idx;
						float    simulation_speed;
						float    range_radius;
					} constants =
					{
						.rain_data_idx = i,
						.simulation_speed = simulation_speed,
						.range_radius = range_radius
					};

					GfxPipelineState* rain_simulation_pso = PSOCache::Get(GfxPipelineStateID::RainSimulation);
					cmd_list->SetPipelineState(rain_simulation_pso);
					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
					cmd_list->Dispatch(DivideAndRoundUp(MAX_RAIN_DATA_BUFFER_SIZE, 256), 1, 1);

				}, RGPassType::Compute, RGPassFlags::None);
		}

		struct RainDrawPassData
		{
			RGBufferReadOnlyId rain_data_buffer;
		};

		rg.AddPass<RainDrawPassData>("Rain Draw Pass",
			[=](RainDrawPassData& data, RenderGraphBuilder& builder)
			{
				data.rain_data_buffer = builder.ReadBuffer(RG_RES_NAME(RainDataBuffer), ReadAccess_NonPixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(FinalTexture), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RainDrawPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { context.GetReadOnlyBuffer(data.rain_data_buffer) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);

				uint32 i = dst_handle.GetIndex();

				struct Constants
				{
					uint32   rain_data_idx;
					uint32   rain_streak_idx;
					float	 rain_streak_scale;

				} constants =
				{
					.rain_data_idx = i,
					.rain_streak_idx = (uint32)rain_streak_handle,
					.rain_streak_scale = streak_scale
				};

				GfxPipelineState* rain_pso = PSOCache::Get(GfxPipelineStateID::Rain);
				cmd_list->SetPipelineState(rain_pso);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Draw(uint32(rain_density * MAX_RAIN_DATA_BUFFER_SIZE) * 6);
			},
			RGPassType::Graphics, RGPassFlags::None);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx("Rain Settings", 0))
				{
					ImGui::Checkbox("Pause simulation", &pause_simulation);
					if (ImGui::Checkbox("Cheap", &cheap))
					{
						OnRainEnabled(true);
					}
					ImGui::SliderFloat("Simulation speed", &simulation_speed, 0.1f, 10.0f);
					ImGui::SliderFloat("Rain density", &rain_density, 0.0f, 1.0f);
					ImGui::SliderFloat("Range radius", &range_radius, 10.0f, 50.0f);
					ImGui::SliderFloat("Streak scale", &streak_scale, 0.01f, 1.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer);
	}

	void RainPass::OnSceneInitialized()
	{
		rain_streak_handle = g_TextureManager.LoadTexture(paths::TexturesDir() + "Rain/RainStreak.dds");
		rain_splash_bump_handle = g_TextureManager.LoadTexture(paths::TexturesDir() + "Rain/SplashBump.dds");
		rain_splash_diffuse_handle = g_TextureManager.LoadTexture(paths::TexturesDir() + "Rain/SplashDiffuse.dds");

		GfxBufferDesc rain_data_buffer_desc = StructuredBufferDesc<RainData>(MAX_RAIN_DATA_BUFFER_SIZE);
		std::vector<RainData> rain_data_buffer_init(MAX_RAIN_DATA_BUFFER_SIZE);

		RealRandomGenerator<float> rng(-1.0f, 1.0f);
		for (uint64 i = 0; i < MAX_RAIN_DATA_BUFFER_SIZE; ++i)
		{
			rain_data_buffer_init[i].position = Vector3(0.0f, -1000.0f, 0.0f);
			rain_data_buffer_init[i].velocity = Vector3(0.0f, -10.0f + rng(), 0.0f);
			rain_data_buffer_init[i].state = 0.0f;
		}
		rain_data_buffer = std::make_unique<GfxBuffer>(gfx, rain_data_buffer_desc, rain_data_buffer_init.data());
	}

}
