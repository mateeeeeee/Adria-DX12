#include "HBAOPass.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Utilities/Random.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	HBAOPass::HBAOPass(uint32 w, uint32 h) : width(w), height(h), hbao_random_texture(nullptr),
		blur_pass(w, h){}

	void HBAOPass::AddPass(RenderGraph& rendergraph)
	{
		struct HBAOPassData
		{
			RGTextureReadOnlyId gbuffer_normal_srv;
			RGTextureReadOnlyId depth_stencil_srv;
			RGTextureReadWriteId output_uav;
		};

		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<HBAOPassData>("HBAO Pass",
			[=](HBAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hbao_desc{};
				hbao_desc.format = EFormat::R8_UNORM;
				hbao_desc.width = width;
				hbao_desc.height = height;

				builder.DeclareTexture(RG_RES_NAME(HBAO_Output), hbao_desc);
				data.output_uav = builder.WriteTexture(RG_RES_NAME(HBAO_Output));
				data.gbuffer_normal_srv = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.depth_stencil_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[&](HBAOPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::HBAO));

				uint32 i = (uint32)descriptor_allocator->AllocateRange(4);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth_stencil_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.gbuffer_normal_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), hbao_random_texture->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadWriteTexture(data.output_uav), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct HBAOConstants
				{
					float32  r2;
					float32  radius_to_screen;
					float32  power;
					float32  noise_scale;

					uint32   depth_idx;
					uint32   normal_idx;
					uint32   noise_idx;
					uint32   output_idx;
				} constants =
				{
					.r2 = params.hbao_radius * params.hbao_radius, .radius_to_screen = params.hbao_radius * 0.5f * float32(height) / (tanf(global_data.camera_fov * 0.5f) * 2.0f),
					.power = params.hbao_power,
					.noise_scale = std::max(width * 1.0f / NOISE_DIM,height * 1.0f / NOISE_DIM),
					.depth_idx = i, .normal_idx = i + 1, .noise_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetComputeRootConstantBufferView(0, global_data.new_frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 8, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute);

		blur_pass.AddPass(rendergraph, RG_RES_NAME(HBAO_Output), RG_RES_NAME(AmbientOcclusion), " HBAO");
		
		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("HBAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderFloat("Power", &params.hbao_power, 1.0f, 16.0f);
					ImGui::SliderFloat("Radius", &params.hbao_radius, 0.25f, 8.0f);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

	void HBAOPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(w, h);
	}

	void HBAOPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		std::vector<float32> random_texture_data;
		for (int32 i = 0; i < 8 * 8; i++)
		{
			float32 rand = rand_float();
			random_texture_data.push_back(sin(rand));
			random_texture_data.push_back(cos(rand));
			random_texture_data.push_back(rand_float());
			random_texture_data.push_back(rand_float());
		}

		TextureInitialData data{};
		data.pData = random_texture_data.data();
		data.RowPitch = 8 * 4 * sizeof(float32);
		data.SlicePitch = 0;

		TextureDesc noise_desc{};
		noise_desc.width = NOISE_DIM;
		noise_desc.height = NOISE_DIM;
		noise_desc.format = EFormat::R32G32B32A32_FLOAT;
		noise_desc.initial_state = EResourceState::PixelShaderResource;
		noise_desc.bind_flags = EBindFlag::ShaderResource;

		hbao_random_texture = std::make_unique<Texture>(gfx, noise_desc, &data);
		hbao_random_texture->CreateSRV();
		hbao_random_texture->GetNative()->SetName(L"HBAO Random Texture");
	}

}

