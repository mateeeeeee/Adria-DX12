#include "SSAOPass.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Utilities/Random.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	SSAOPass::SSAOPass(uint32 w, uint32 h) : width(w), height(h), ssao_random_texture(nullptr),
		blur_pass(w, h)
	{
	}
	void SSAOPass::AddPass(RenderGraph& rendergraph)
	{
		struct SSAOPassData
		{
			RGTextureReadOnlyId gbuffer_normal_srv;
			RGTextureReadOnlyId depth_stencil_srv;
		};

		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<SSAOPassData>("SSAO Pass",
			[=](SSAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = EFormat::R8_UNORM;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_RES_NAME(SSAO_Output), render_target_desc);
				builder.WriteRenderTarget(RG_RES_NAME(SSAO_Output), ERGLoadStoreAccessOp::Discard_Preserve);
				data.gbuffer_normal_srv = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.depth_stencil_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[&](SSAOPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::AO));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::SSAO));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.gbuffer_normal_srv), context.GetReadOnlyTexture(data.depth_stencil_srv), ssao_random_texture->GetSRV() };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor };
				UINT src_range_sizes[] = { 1, 1, 1 };
				UINT dst_range_sizes[] = { 3 };
				device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor);

				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}
			);

		blur_pass.AddPass(rendergraph, RG_RES_NAME(SSAO_Output), RG_RES_NAME(AmbientOcclusion), " SSAO");
		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderFloat("Power", &params.ssao_power, 1.0f, 16.0f);
					ImGui::SliderFloat("Radius", &params.ssao_radius, 0.5f, 4.0f);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

	void SSAOPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(w, h);
	}

	void SSAOPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		std::vector<float32> random_texture_data;
		for (int32 i = 0; i < 8 * 8; i++)
		{
			random_texture_data.push_back(rand_float()); 
			random_texture_data.push_back(rand_float());
			random_texture_data.push_back(0.0f);
			random_texture_data.push_back(1.0f);
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

		ssao_random_texture = std::make_unique<Texture>(gfx, noise_desc, &data);
		ssao_random_texture->CreateSRV();
		ssao_random_texture->GetNative()->SetName(L"SSAO Random Texture");
	}

}

