#include "TiledLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Logging/Logger.h"
#include "../Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	TiledLightingPass::TiledLightingPass(entt::registry& reg, uint32 w, uint32 h) : reg(reg), width(w), height(h), 
		add_textures_pass(width, height), copy_to_texture_pass(width, height)
	{}

	void TiledLightingPass::AddPass(RenderGraph& rendergraph)
	{
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct TiledLightingPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  depth;
			RGTextureReadWriteId tiled_target;
			RGTextureReadWriteId tiled_debug_target;
		};

		rendergraph.AddPass<TiledLightingPassData>("Tiled Lighting Pass",
			[=](TiledLightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc tiled_desc{};
				tiled_desc.width = width;
				tiled_desc.height = height;
				tiled_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(TiledTarget), tiled_desc);
				builder.DeclareTexture(RG_RES_NAME(TiledDebugTarget), tiled_desc);

				data.tiled_target = builder.WriteTexture(RG_RES_NAME(TiledTarget));
				data.tiled_debug_target = builder.WriteTexture(RG_RES_NAME(TiledDebugTarget));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](TiledLightingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto dynamic_allocator = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::TiledLighting));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::TiledLighting));

				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(1, global_data.compute_cbuffer_address);

				//t0,t1,t2 - gbuffer and depth
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal), 
																  context.GetReadOnlyTexture(data.gbuffer_albedo), 
																  context.GetReadOnlyTexture(data.depth) };
					uint32 src_range_sizes[] = { 1,1,1 };

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetComputeRootDescriptorTable(2, dst_descriptor);
				}

				D3D12_GPU_DESCRIPTOR_HANDLE uav_target_for_clear{};
				D3D12_GPU_DESCRIPTOR_HANDLE uav_debug_for_clear{};
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadWriteTexture(data.tiled_target), context.GetReadWriteTexture(data.tiled_debug_target) };
					uint32 src_range_sizes[] = { 1,1 };

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetComputeRootDescriptorTable(3, dst_descriptor);
					uav_target_for_clear = descriptor_allocator->GetHandle(descriptor_index);
					uav_debug_for_clear = descriptor_allocator->GetHandle(descriptor_index + 1);
				}
				cmd_list->SetComputeRootDescriptorTable(4, global_data.lights_buffer_gpu_srv);

				static constexpr float black[4] = { 0.0f,0.0f,0.0f,0.0f };

				Texture const& tiled_target = context.GetTexture(data.tiled_target.GetResourceId());
				cmd_list->ClearUnorderedAccessViewFloat(uav_target_for_clear, context.GetReadWriteTexture(data.tiled_target), tiled_target.GetNative(),
					black, 0, nullptr);
				Texture const& tiled_debug_target = context.GetTexture(data.tiled_debug_target.GetResourceId());
				cmd_list->ClearUnorderedAccessViewFloat(uav_debug_for_clear, context.GetReadWriteTexture(data.tiled_debug_target), tiled_debug_target.GetNative(),
					black, 0, nullptr);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		if (visualize_tiled)  add_textures_pass.AddPass(rendergraph, RG_RES_NAME(HDR_RenderTarget), RG_RES_NAME(TiledTarget), RG_RES_NAME(TiledDebugTarget), EBlendMode::AlphaBlend);
		else copy_to_texture_pass.AddPass(rendergraph, RG_RES_NAME(HDR_RenderTarget), RG_RES_NAME(TiledTarget), EBlendMode::AdditiveBlend);

		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Tiled Deferred", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::Checkbox("Visualize Tiles", &visualize_tiled);
					if (visualize_tiled) ImGui::SliderInt("Visualize Scale", &visualize_max_lights, 1, 32);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

}

