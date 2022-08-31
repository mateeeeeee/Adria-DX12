#include "TiledLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "../Logging/Logger.h"

using namespace DirectX;

namespace adria
{

	TiledLightingPass::TiledLightingPass(entt::registry& reg, uint32 w, uint32 h) : reg(reg), width(w), height(h)
	{
	}

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

				auto light_view = reg.view<Light>();
				std::vector<StructuredLight> structured_lights{};
				for (auto e : light_view)
				{
					StructuredLight structured_light{};
					auto& light = light_view.get<Light>(e);
					structured_light.color = light.color * light.energy;
					structured_light.position = XMVector4Transform(light.position, global_data.camera_view);
					structured_light.direction = XMVector4Transform(light.direction, global_data.camera_view);
					structured_light.range = light.range;
					structured_light.type = static_cast<int>(light.type);
					structured_light.inner_cosine = light.inner_cosine;
					structured_light.outer_cosine = light.outer_cosine;
					structured_light.active = light.active;
					structured_light.casts_shadows = light.casts_shadows;

					structured_lights.push_back(structured_light);
				}

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

				{
					uint64 alloc_size = sizeof(StructuredLight) * structured_lights.size();
					DynamicAllocation dynamic_alloc = dynamic_allocator->Allocate(alloc_size, sizeof(StructuredLight));
					dynamic_alloc.Update(structured_lights.data(), alloc_size);

					auto i = descriptor_allocator->Allocate();

					D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
					desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
					desc.Format = DXGI_FORMAT_UNKNOWN;
					desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
					desc.Buffer.StructureByteStride = sizeof(StructuredLight);
					desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
					desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;

					auto handle = descriptor_allocator->GetHandle(i);
					device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, handle);
					cmd_list->SetComputeRootDescriptorTable(4, handle);
				}

				static constexpr float32 black[4] = { 0.0f,0.0f,0.0f,0.0f };

				Texture const& tiled_target = context.GetTexture(data.tiled_target.GetResourceId());
				cmd_list->ClearUnorderedAccessViewFloat(uav_target_for_clear, context.GetReadWriteTexture(data.tiled_target), tiled_target.GetNative(),
					black, 0, nullptr);
				Texture const& tiled_debug_target = context.GetTexture(data.tiled_debug_target.GetResourceId());
				cmd_list->ClearUnorderedAccessViewFloat(uav_debug_for_clear, context.GetReadWriteTexture(data.tiled_debug_target), tiled_debug_target.GetNative(),
					black, 0, nullptr);
				cmd_list->Dispatch(std::ceil(width / 16.0f), std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

}

