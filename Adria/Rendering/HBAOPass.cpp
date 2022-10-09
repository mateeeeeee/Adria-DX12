#include "HBAOPass.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Utilities/Random.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	HBAOPass::HBAOPass(uint32 w, uint32 h) : width(w), height(h), hbao_random_texture(nullptr),
		blur_pass(w, h)
	{
	}

	void HBAOPass::AddPass(RenderGraph& rendergraph)
	{
		struct HBAOPassData
		{
			RGTextureReadOnlyId gbuffer_normal_srv;
			RGTextureReadOnlyId depth_stencil_srv;
		};

		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<HBAOPassData>("HBAO Pass",
			[=](HBAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = EFormat::R8_UNORM;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_RES_NAME(HBAO_Output), render_target_desc);
				builder.WriteRenderTarget(RG_RES_NAME(HBAO_Output), ERGLoadStoreAccessOp::Discard_Preserve);
				data.gbuffer_normal_srv = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.depth_stencil_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[&](HBAOPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::AO));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::HBAO));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.gbuffer_normal_srv), context.GetReadOnlyTexture(data.depth_stencil_srv), hbao_random_texture->GetSRV() };
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

