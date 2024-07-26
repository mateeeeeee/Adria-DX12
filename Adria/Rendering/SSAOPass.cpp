#include "SSAOPass.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Math/Packing.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/Random.h"
#include "Core/ConsoleVariable.h"
#include "Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{
	namespace cvars
	{
		static ConsoleVariable ssao_power("r.SSAO.Power", 1.5f);
		static ConsoleVariable ssao_radius("r.SSAO.Radius", 1.0f);
	}

	SSAOPass::SSAOPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), ssao_random_texture(nullptr),
		blur_pass(gfx, w >> resolution, h >> resolution)
	{
		ADRIA_CVAR_CALLBACK(ssao_power,  (float v) { params.ssao_power = v; });
		ADRIA_CVAR_CALLBACK(ssao_radius, (float v) { params.ssao_radius = v; });

		CreatePSO();
		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		for (uint32 i = 0; i < ARRAYSIZE(ssao_kernel); i++)
		{
			Vector4 offset(2 * rand_float() - 1, 2 * rand_float() - 1, rand_float(), 0.0f);
			offset.Normalize();
			offset *= rand_float();
			ssao_kernel[i] = offset;
		}
	}
	void SSAOPass::AddPass(RenderGraph& rendergraph)
	{
		struct SSAOPassData
		{
			RGTextureReadOnlyId gbuffer_normal;
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		rendergraph.AddPass<SSAOPassData>("SSAO Pass",
			[=](SSAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc ssao_desc{};
				ssao_desc.format = GfxFormat::R8_UNORM;
				ssao_desc.width = width >> resolution;
				ssao_desc.height = height >> resolution;

				builder.DeclareTexture(RG_RES_NAME(SSAO_Output), ssao_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(SSAO_Output));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[&](SSAOPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadOnlyTexture(data.gbuffer_normal),
					ssao_random_texture_srv,
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct SSAOConstants
				{
					uint32 ssao_params_packed;
					uint32 resolution_factor;
					float  noise_scale_x;
					float  noise_scale_y;
		
					uint32   depth_idx;
					uint32   normal_idx;
					uint32   noise_idx;
					uint32   output_idx;
				} constants = 
				{
					.ssao_params_packed = PackTwoFloatsToUint32(params.ssao_radius,params.ssao_power), .resolution_factor = (uint32)resolution,
					.noise_scale_x = (width >> resolution) * 1.0f / NOISE_DIM, .noise_scale_y = (height >> resolution) * 1.0f / NOISE_DIM,
					.depth_idx = i, .normal_idx = i + 1, .noise_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetPipelineState(ssao_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, ssao_kernel);
				cmd_list->Dispatch(DivideAndRoundUp((width >> resolution), 16), DivideAndRoundUp((height >> resolution), 16), 1);

			}, RGPassType::Compute);

		blur_pass.AddPass(rendergraph, RG_RES_NAME(SSAO_Output), RG_RES_NAME(AmbientOcclusion), " SSAO");

		GUI_Command([&]() 
			{
				if (ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderFloat("Power", &params.ssao_power, 1.0f, 16.0f);
					ImGui::SliderFloat("Radius", &params.ssao_radius, 0.5f, 4.0f);

					static int _resolution = (int)resolution;
					if (ImGui::Combo("SSAO Resolution", &_resolution, "Full\0Half\0Quarter\0", 3))
					{
						resolution = (SSAOResolution)_resolution;
						OnResize(width, height);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessor
		);
	}

	void SSAOPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(w >> resolution, h >> resolution);
	}

	void SSAOPass::OnSceneInitialized()
	{
		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		std::vector<float> random_texture_data;
		for (int32 i = 0; i < 8 * 8; i++)
		{
			random_texture_data.push_back(rand_float()); 
			random_texture_data.push_back(rand_float());
			random_texture_data.push_back(0.0f);
			random_texture_data.push_back(1.0f);
		}

		GfxTextureInitialData data{};
		data.data = random_texture_data.data();
		data.row_pitch = 8 * 4 * sizeof(float);
		data.slice_pitch = 0;

		GfxTextureDesc noise_desc{};
		noise_desc.width = NOISE_DIM;
		noise_desc.height = NOISE_DIM;
		noise_desc.format = GfxFormat::R32G32B32A32_FLOAT;
		noise_desc.initial_state = GfxResourceState::PixelSRV;
		noise_desc.bind_flags = GfxBindFlag::ShaderResource;

		ssao_random_texture = gfx->CreateTexture(noise_desc, &data);
		ssao_random_texture->SetName("SSAO Random Texture");
		ssao_random_texture_srv = gfx->CreateTextureSRV(ssao_random_texture.get());
	}

	void SSAOPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Ssao;
		ssao_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

