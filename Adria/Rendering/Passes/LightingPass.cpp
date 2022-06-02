#include "LightingPass.h"
#include "../ConstantBuffers.h"
#include "../Components.h"
#include "../RendererGlobalData.h"
#include "../RootSigPSOManager.h"
#include "../../RenderGraph/RenderGraph.h"
#include "../../Graphics/GPUProfiler.h"

using namespace DirectX;

namespace adria
{

	LightingPassData const& LightingPass::AddPass(RenderGraph& rendergraph, Light const& light_data,
		RGTextureRTVRef hdr_rtv,
		RGTextureSRVRef gbuffer_normal_srv,
		RGTextureSRVRef gbuffer_albedo_srv,
		RGTextureSRVRef depth_stencil_srv)
	{
		RendererGlobalData const& global_data = rendergraph.GetBlackboard().GetChecked<RendererGlobalData>();

		//ShadowData = shadow_pass.AddPass(rendergraph, Light, ...);

		return rendergraph.AddPass<LightingPassData>("Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				builder.RenderTarget(hdr_rtv, ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.Read(gbuffer_normal_srv.GetTypedResourceHandle());
				builder.Read(gbuffer_albedo_srv.GetTypedResourceHandle());
				builder.Read(depth_stencil_srv.GetTypedResourceHandle());
				builder.SetViewport(width, height);
			},
			[=](LightingPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				LightCBuffer light_cbuf_data{};
				light_cbuf_data.active = light_data.active;
				light_cbuf_data.casts_shadows = light_data.casts_shadows;
				light_cbuf_data.color = light_data.color * light_data.energy;
				light_cbuf_data.direction = light_data.direction;
				light_cbuf_data.inner_cosine = light_data.inner_cosine;
				light_cbuf_data.outer_cosine = light_data.outer_cosine;
				light_cbuf_data.position = light_data.position;
				light_cbuf_data.range = light_data.range;
				light_cbuf_data.type = static_cast<int32>(light_data.type);
				light_cbuf_data.use_cascades = light_data.use_cascades;
				light_cbuf_data.volumetric = light_data.volumetric;
				light_cbuf_data.volumetric_strength = light_data.volumetric_strength;
				light_cbuf_data.sscs = light_data.sscs;
				light_cbuf_data.sscs_thickness = light_data.sscs_thickness;
				light_cbuf_data.sscs_max_ray_distance = light_data.sscs_max_ray_distance;
				light_cbuf_data.sscs_max_depth_distance = light_data.sscs_max_depth_distance;

				XMMATRIX camera_view = global_data.camera_view;
				light_cbuf_data.position = DirectX::XMVector4Transform(light_cbuf_data.position, camera_view);
				light_cbuf_data.direction = DirectX::XMVector4Transform(light_cbuf_data.direction, camera_view);

				DynamicAllocation light_allocation = dynamic_allocator->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				light_allocation.Update(light_cbuf_data);

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::LightingPBR));
				cmd_list->SetPipelineState(light_data.ray_traced_shadows ?
					RootSigPSOManager::GetPipelineState(EPipelineStateObject::LightingPBR_RayTracedShadows) :
					RootSigPSOManager::GetPipelineState(EPipelineStateObject::LightingPBR));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);

				cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);

				//cmd_list->SetGraphicsRootConstantBufferView(2, shadow_allocation.gpu_address);

				//t0,t1,t2 - gbuffer and depth
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { resources.GetSRV(gbuffer_normal_srv), resources.GetSRV(gbuffer_albedo_srv), resources.GetSRV(depth_stencil_srv) };
					uint32 src_range_sizes[] = { 1,1,1 };

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetGraphicsRootDescriptorTable(3, dst_descriptor);
				}

				//t4,t5,t6 - shadow maps
				/* {
					D3D12_CPU_DESCRIPTOR_HANDLE shadow_cpu_handles[] = { null_heap->GetHandle(TEXTURE2D_SLOT),
						null_heap->GetHandle(TEXTURECUBE_SLOT), null_heap->GetHandle(TEXTURE2DARRAY_SLOT),
						null_heap->GetHandle(TEXTURE2D_SLOT) };
					uint32 src_range_sizes[] = { 1,1,1,1 };

					if (light_data.ray_traced_shadows)
					{
						shadow_cpu_handles[3] = ray_tracer.GetRayTracingShadowsTexture().SRV();
					}
					else if (light_data.casts_shadows)
					{
						switch (light_data.type)
						{
						case ELightType::Directional:
							if (light_data.use_cascades) shadow_cpu_handles[2] = shadow_depth_cascades->SRV();
							else shadow_cpu_handles[0] = shadow_depth_map->SRV();
							break;
						case ELightType::Spot:
							shadow_cpu_handles[0] = shadow_depth_map->SRV();
							break;
						case ELightType::Point:
							shadow_cpu_handles[1] = shadow_depth_cubemap->SRV();
							break;
						default:
							ADRIA_ASSERT(false);
						}
					}

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(shadow_cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(shadow_cpu_handles) };
					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(shadow_cpu_handles), shadow_cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetGraphicsRootDescriptorTable(4, dst_descriptor);
				} */

				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

				//VOLUMETRIC
			});

	}

}

