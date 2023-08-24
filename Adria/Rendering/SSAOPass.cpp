#include "SSAOPass.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Math/Packing.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/Random.h"
#include "Core/ConsoleVariable.h"
#include "Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{
	namespace cvars
	{
		static ConsoleVariable ssao_power("ssao.power",   1.5f);
		static ConsoleVariable ssao_radius("ssao.radius", 1.0f);
	}

	SSAOPass::SSAOPass(uint32 w, uint32 h) : width(w), height(h), ssao_random_texture(nullptr),
		blur_pass(w >> resolution, h >> resolution)
	{
		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		for (uint32 i = 0; i < ARRAYSIZE(ssao_kernel); i++)
		{
			DirectX::XMFLOAT4 _offset = DirectX::XMFLOAT4(2 * rand_float() - 1, 2 * rand_float() - 1, rand_float(), 0.0f);
			DirectX::XMVECTOR offset = DirectX::XMLoadFloat4(&_offset);
			offset = DirectX::XMVector4Normalize(offset);
			offset *= rand_float();
			ssao_kernel[i] = offset;
		}
	}
	void SSAOPass::AddPass(RenderGraph& rendergraph)
	{
		struct SSAOPassData
		{
			RGTextureReadOnlyId gbuffer_normal_srv;
			RGTextureReadOnlyId depth_stencil_srv;
			RGTextureReadWriteId output_uav;
		};

		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		rendergraph.AddPass<SSAOPassData>("SSAO Pass",
			[=](SSAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc ssao_desc{};
				ssao_desc.format = GfxFormat::R8_UNORM;
				ssao_desc.width = width >> resolution;
				ssao_desc.height = height >> resolution;

				builder.DeclareTexture(RG_RES_NAME(SSAO_Output), ssao_desc);
				data.output_uav = builder.WriteTexture(RG_RES_NAME(SSAO_Output));
				data.gbuffer_normal_srv = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.depth_stencil_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[&](SSAOPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::SSAO));

				uint32 i = gfx->AllocateDescriptorsGPU(4).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.depth_stencil_srv));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadOnlyTexture(data.gbuffer_normal_srv));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 2), ssao_random_texture_srv);
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 3), ctx.GetReadWriteTexture(data.output_uav));

				struct SSAOConstants
				{
					uint32 ssao_params_packed;
					uint32 resolutionFactor;
					float  noise_scale_x;
					float  noise_scale_y;
		
					uint32   depth_idx;
					uint32   normal_idx;
					uint32   noise_idx;
					uint32   output_idx;
				} constants = 
				{
					.ssao_params_packed = PackTwoFloatsToUint32(params.ssao_radius,params.ssao_power), .resolutionFactor = (uint32)resolution,
					.noise_scale_x = (width >> resolution) * 1.0f / NOISE_DIM, .noise_scale_y = (height >> resolution) * 1.0f / NOISE_DIM,
					.depth_idx = i, .normal_idx = i + 1, .noise_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, ssao_kernel);
				cmd_list->Dispatch((uint32)std::ceil((width >> resolution) / 16.0f), (uint32)std::ceil((height >> resolution) / 16.0f), 1);

				GUI_DisplayTexture("ssao", &ctx.GetTexture(*data.output_uav));
			}, RGPassType::Compute);

		blur_pass.AddPass(rendergraph, RG_RES_NAME(SSAO_Output), RG_RES_NAME(AmbientOcclusion), " SSAO");
		
		GUI_DisplayTexture("random", ssao_random_texture.get());
		params.ssao_power = std::clamp(cvars::ssao_power.Get(), 1.0f, 16.0f);
		params.ssao_radius = std::clamp(cvars::ssao_radius.Get(), 0.5f, 4.0f);
		GUI_RunCommand([&]() 
			{
				if (ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderFloat("Power", &cvars::ssao_power.Get(), 1.0f, 16.0f);
					ImGui::SliderFloat("Radius", &cvars::ssao_radius.Get(), 0.5f, 4.0f);

					static int _resolution = 1;
					static const char* res_types[] = { "Full", "Half", "Quarter" };
					const char* res_combo_label = res_types[_resolution];
					if (ImGui::BeginCombo("SSAO Resolution", res_combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(res_types); n++)
						{
							const bool is_selected = (_resolution == n);
							if (ImGui::Selectable(res_types[n], is_selected)) _resolution = (SSAOResolution)n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (resolution != _resolution)
					{
						resolution = (SSAOResolution)_resolution;
						OnResize(width, height);
					}

					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

	void SSAOPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(w >> resolution, h >> resolution);
	}

	void SSAOPass::OnSceneInitialized(GfxDevice* gfx)
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
		data.pData = random_texture_data.data();
		data.RowPitch = 8 * 4 * sizeof(float);
		data.SlicePitch = 0;

		GfxTextureDesc noise_desc{};
		noise_desc.width = NOISE_DIM;
		noise_desc.height = NOISE_DIM;
		noise_desc.format = GfxFormat::R32G32B32A32_FLOAT;
		noise_desc.initial_state = GfxResourceState::PixelShaderResource;
		noise_desc.bind_flags = GfxBindFlag::ShaderResource;

		ssao_random_texture = std::make_unique<GfxTexture>(gfx, noise_desc, &data);
		ssao_random_texture->SetName("SSAO Random Texture");
		ssao_random_texture_srv = gfx->CreateTextureSRV(ssao_random_texture.get());
	}
}

