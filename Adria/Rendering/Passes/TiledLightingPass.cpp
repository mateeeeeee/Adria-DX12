#include "TiledLightingPass.h"
#include "../ConstantBuffers.h"
#include "../Components.h"
#include "../GlobalBlackboardData.h"
#include "../RootSigPSOManager.h"
#include "../../RenderGraph/RenderGraph.h"
#include "../../Graphics/GPUProfiler.h"
#include "../../tecs/registry.h"
#include "../../Logging/Logger.h"
#include "pix3.h"

using namespace DirectX;

namespace adria
{
	TiledLightingPassData const& TiledLightingPass::AddPass(RenderGraph& rendergraph,
		RGTextureSRVRef gbuffer_normal_srv,
		RGTextureSRVRef gbuffer_albedo_srv,
		RGTextureSRVRef depth_stencil_srv)
	{
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();

		return rendergraph.AddPass<TiledLightingPassData>("Tiled Lighting Pass",
			[=](TiledLightingPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc tiled_desc{};
				tiled_desc.width = width;
				tiled_desc.height = height;
				tiled_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				tiled_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
				tiled_desc.initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

				RGTextureRef tiled_target = builder.CreateTexture("Tiled UAV target", tiled_desc);
				data.tiled_uav = builder.CreateUAV(tiled_target);
				data.tiled_srv = builder.CreateSRV(tiled_target);

				TextureDesc tiled_debug_desc{};
				tiled_debug_desc.width = width;
				tiled_debug_desc.height = height;
				tiled_debug_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				tiled_debug_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
				tiled_debug_desc.initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; //?

				RGTextureRef tiled_debug_target = builder.CreateTexture("Tiled UAV target", tiled_debug_desc);
				data.tiled_debug_uav = builder.CreateUAV(tiled_debug_target);
				data.tiled_debug_srv = builder.CreateSRV(tiled_debug_target);

				builder.Read(gbuffer_normal_srv.GetResourceHandle(), ReadAccess_NonPixelShader);
				builder.Read(gbuffer_albedo_srv.GetResourceHandle(), ReadAccess_NonPixelShader);
				builder.Read(depth_stencil_srv.GetResourceHandle(), ReadAccess_NonPixelShader);
				builder.SetViewport(width, height);

				builder.Write(tiled_target, WriteAccess_Unordered);
				builder.Write(tiled_debug_target, WriteAccess_Unordered);
			},
			[=](TiledLightingPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto upload_buffer = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				auto light_view = reg.view<Light>();
				std::vector<StructuredLight> structured_lights{};
				for (auto e : light_view)
				{
					StructuredLight structured_light{};
					auto& light = light_view.get(e);
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

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::TiledLighting));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::TiledLighting));

				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(1, global_data.compute_cbuffer_address);

				//t0,t1,t2 - gbuffer and depth
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { resources.GetSRV(gbuffer_normal_srv), resources.GetSRV(gbuffer_albedo_srv),resources.GetSRV(depth_stencil_srv) };
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
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { resources.GetUAV(data.tiled_uav), resources.GetUAV(data.tiled_debug_uav) };
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
					DynamicAllocation dynamic_alloc = upload_buffer->Allocate(alloc_size, sizeof(StructuredLight));
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

				float32 black[4] = { 0.0f,0.0f,0.0f,0.0f };

				Texture& tiled_target = resources.GetTexture(data.tiled_uav.GetResourceHandle());
				cmd_list->ClearUnorderedAccessViewFloat(uav_target_for_clear, resources.GetUAV(data.tiled_uav), tiled_target.GetNative(),
					black, 0, nullptr);
				Texture& tiled_debug_target = resources.GetTexture(data.tiled_debug_uav.GetResourceHandle());
				cmd_list->ClearUnorderedAccessViewFloat(uav_debug_for_clear, resources.GetUAV(data.tiled_debug_uav), tiled_debug_target.GetNative(),
					black, 0, nullptr);
				cmd_list->Dispatch((uint32)std::ceil(width * 1.0f / 16), (uint32)(height * 1.0f / 16), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
	}
}

