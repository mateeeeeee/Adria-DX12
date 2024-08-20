#include "HBAOPass.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/Random.h"
#include "Editor/GUICommand.h"

namespace adria
{
	
	HBAOPass::HBAOPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), hbao_random_texture(nullptr),
		blur_pass(gfx)
	{
		CreatePSO();
	}

	void HBAOPass::AddPass(RenderGraph& rendergraph)
	{
		struct HBAOPassData
		{
			RGTextureReadOnlyId gbuffer_normal_srv;
			RGTextureReadOnlyId depth_stencil_srv;
			RGTextureReadWriteId output_uav;
		};

		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		rendergraph.AddPass<HBAOPassData>("HBAO Pass",
			[=](HBAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hbao_desc{};
				hbao_desc.format = GfxFormat::R8_UNORM;
				hbao_desc.width = width;
				hbao_desc.height = height;

				builder.DeclareTexture(RG_NAME(HBAO_Output), hbao_desc);
				data.output_uav = builder.WriteTexture(RG_NAME(HBAO_Output));
				data.gbuffer_normal_srv = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.depth_stencil_srv = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[&](HBAOPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth_stencil_srv),
					ctx.GetReadOnlyTexture(data.gbuffer_normal_srv),
					hbao_random_texture_srv,
					ctx.GetReadWriteTexture(data.output_uav)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct HBAOConstants
				{
					float  r2;
					float  radius_to_screen;
					float  power;
					float  noise_scale;

					uint32   depth_idx;
					uint32   normal_idx;
					uint32   noise_idx;
					uint32   output_idx;
				} constants =
				{
					.r2 = params.hbao_radius * params.hbao_radius, .radius_to_screen = params.hbao_radius * 0.5f * float(height) / (tanf(frame_data.camera_fov * 0.5f) * 2.0f),
					.power = params.hbao_power,
					.noise_scale = std::max(width * 1.0f / NOISE_DIM,height * 1.0f / NOISE_DIM),
					.depth_idx = i, .normal_idx = i + 1, .noise_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetPipelineState(hbao_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);

		blur_pass.AddPass(rendergraph, RG_NAME(HBAO_Output), RG_NAME(AmbientOcclusion), " HBAO");
	}

	void HBAOPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("HBAO", ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderFloat("Power", &params.hbao_power, 1.0f, 16.0f);
					ImGui::SliderFloat("Radius", &params.hbao_radius, 0.25f, 8.0f);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_AO);
	}

	void HBAOPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void HBAOPass::OnSceneInitialized()
	{
		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		std::vector<float> random_texture_data;
		for (int32 i = 0; i < 8 * 8; i++)
		{
			float rand = rand_float();
			random_texture_data.push_back(sin(rand));
			random_texture_data.push_back(cos(rand));
			random_texture_data.push_back(rand_float());
			random_texture_data.push_back(rand_float());
		}

		GfxTextureSubData data{};
		data.data = random_texture_data.data();
		data.row_pitch = 8 * 4 * sizeof(float);
		data.slice_pitch = 0;

		GfxTextureData init_data{};
		init_data.sub_data = &data;
		init_data.sub_count = 1;

		GfxTextureDesc noise_desc{};
		noise_desc.width = NOISE_DIM;
		noise_desc.height = NOISE_DIM;
		noise_desc.format = GfxFormat::R32G32B32A32_FLOAT;
		noise_desc.initial_state = GfxResourceState::PixelSRV;
		noise_desc.bind_flags = GfxBindFlag::ShaderResource;

		hbao_random_texture = gfx->CreateTexture(noise_desc, init_data);
		hbao_random_texture->SetName("HBAO Random Texture");
		hbao_random_texture_srv = gfx->CreateTextureSRV(hbao_random_texture.get());
	}

	void HBAOPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Hbao;
		hbao_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

