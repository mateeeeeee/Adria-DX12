#include "DeferredLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Logging/Logger.h"
#include "pix3.h"

using namespace DirectX;

namespace adria
{

	void DeferredLightingPass::AddPass(RenderGraph& rendergraph)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  depth;
			RGTextureReadWriteId output;
		};

		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<LightingPassData>("Deferred Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				data.output			= builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.depth			= builder.ReadTexture(RG_RES_NAME(DepthStencil),  ReadAccess_NonPixelShader);

				//add shadow maps/ray traced shadows later
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal), 
															  context.GetReadOnlyTexture(data.gbuffer_albedo),
															  context.GetReadOnlyTexture(data.depth),
															  context.GetReadWriteTexture(data.output) };
				uint32 src_range_sizes[] = { 1,1,1,1 };
				uint32 i = (uint32)descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(i);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct DeferredLightingConstants
				{
					uint32 normal_metallic_idx;
					uint32 diffuse_idx;
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.normal_metallic_idx = i, .diffuse_idx = i + 1, .depth_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::DeferredLighting));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.new_frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 4, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);

				/*if (light.volumetric)
				{
					PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Volumetric Lighting Pass");

					ID3D12Device* device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

					if (light.type == ELightType::Directional && !light.casts_shadows)
					{
						ADRIA_LOG(WARNING, "Calling PassVolumetric on a Directional Light that does not cast shadows does not make sense!");
						return;
					}

					cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Volumetric));
					cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);
					if (light.casts_shadows) cmd_list->SetGraphicsRootConstantBufferView(2, context.GetAllocation(data.shadow_alloc).gpu_address);

					DescriptorCPU cpu_handles[] = { context.GetReadOnlyTexture(data.depth), {} };
					uint32 src_range_sizes[] = { 1,1 };

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };

					switch (light.type)
					{
					case ELightType::Directional:
						if (light.use_cascades)
						{
							cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Volumetric_DirectionalCascades));
							cpu_handles[1] = data.shadow_map.IsValid() ? context.GetReadOnlyTexture(data.shadow_map) : global_data.null_srv_texture2darray;
						}
						else
						{
							cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Volumetric_Directional));
							cpu_handles[1] = data.shadow_map.IsValid() ? context.GetReadOnlyTexture(data.shadow_map) : global_data.null_srv_texture2d;
						}
						break;
					case ELightType::Spot:
						cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Volumetric_Spot));
						cpu_handles[1] = data.shadow_map.IsValid() ? context.GetReadOnlyTexture(data.shadow_map) : global_data.null_srv_texture2d;
						break;
					case ELightType::Point:
						cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Volumetric_Point));
						cpu_handles[1] = data.shadow_map.IsValid() ? context.GetReadOnlyTexture(data.shadow_map) : global_data.null_srv_texturecube;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Light Type!");
					}

					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetGraphicsRootDescriptorTable(4, dst_descriptor);
					cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					cmd_list->DrawInstanced(4, 1, 0, 0);
				}*/
			}, ERGPassType::Compute);

	}

}

