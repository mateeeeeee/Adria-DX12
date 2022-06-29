#include "LightingPass.h"
//#include "ShadowPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "../Logging/Logger.h"
#include "pix3.h"

using namespace DirectX;

namespace adria
{

	void LightingPass::AddPass(RenderGraph& rendergraph, Light const& light, size_t light_id)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId gbuffer_normal;
			RGTextureReadOnlyId gbuffer_albedo;
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId shadow_map;
			RGTextureReadOnlyId ray_traced_shadows;
			RGAllocationId		shadow_alloc;
		};

		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<LightingPassData>("Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Preserve_Preserve);
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_PixelShader);
				data.depth			= builder.ReadTexture(RG_RES_NAME(DepthStencil),  ReadAccess_PixelShader);

				if (builder.IsTextureDeclared(RG_RES_NAME_IDX(RayTracedShadows, light_id)))
				{
					data.ray_traced_shadows = builder.ReadTexture(RG_RES_NAME_IDX(RayTracedShadows, light_id), ReadAccess_PixelShader);
				}
				else if (builder.IsTextureDeclared(RG_RES_NAME_IDX(ShadowMap, light_id)))
				{
					data.shadow_map = builder.ReadTexture(RG_RES_NAME_IDX(ShadowMap, light_id), ReadAccess_PixelShader);
					data.shadow_alloc = builder.UseAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id));
				}
				else
				{
					data.shadow_map.Invalidate();
					data.shadow_alloc.Invalidate();
				}
				builder.SetViewport(width, height);
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				LightCBuffer light_cbuf_data{};
				light_cbuf_data.active = light.active;
				light_cbuf_data.casts_shadows = light.casts_shadows;
				light_cbuf_data.color = light.color * light.energy;
				light_cbuf_data.direction = light.direction;
				light_cbuf_data.inner_cosine = light.inner_cosine;
				light_cbuf_data.outer_cosine = light.outer_cosine;
				light_cbuf_data.position = light.position;
				light_cbuf_data.range = light.range;
				light_cbuf_data.type = static_cast<int32>(light.type);
				light_cbuf_data.use_cascades = light.use_cascades;
				light_cbuf_data.volumetric = light.volumetric;
				light_cbuf_data.volumetric_strength = light.volumetric_strength;
				light_cbuf_data.sscs = light.sscs;
				light_cbuf_data.sscs_thickness = light.sscs_thickness;
				light_cbuf_data.sscs_max_ray_distance = light.sscs_max_ray_distance;
				light_cbuf_data.sscs_max_depth_distance = light.sscs_max_depth_distance;

				XMMATRIX camera_view = global_data.camera_view;
				light_cbuf_data.position = DirectX::XMVector4Transform(light_cbuf_data.position, camera_view);
				light_cbuf_data.direction = DirectX::XMVector4Transform(light_cbuf_data.direction, camera_view);

				DynamicAllocation light_allocation = dynamic_allocator->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				light_allocation.Update(light_cbuf_data);

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::LightingPBR));
				cmd_list->SetPipelineState(light.ray_traced_shadows ?
					PSOCache::Get(EPipelineState::LightingPBR_RayTracedShadows) :
					PSOCache::Get(EPipelineState::LightingPBR));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);
				if(light.casts_shadows) cmd_list->SetGraphicsRootConstantBufferView(2, context.GetAllocation(data.shadow_alloc).gpu_address);

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

					cmd_list->SetGraphicsRootDescriptorTable(3, dst_descriptor);
				}

				{
					Descriptor shadow_cpu_handles[] =
					{
						global_data.null_srv_texture2d,
						global_data.null_srv_texturecube,
						global_data.null_srv_texture2darray,
						global_data.null_srv_texture2d };
					uint32 src_range_sizes[] = { 1,1,1,1 };

					if (light.ray_traced_shadows)
					{
						Descriptor ray_traced_shadows = context.GetReadOnlyTexture(data.ray_traced_shadows);
						shadow_cpu_handles[3] = ray_traced_shadows;
					}
					else if (light.casts_shadows)
					{
						ADRIA_ASSERT(data.shadow_map.IsValid());
						Descriptor shadow_map = context.GetReadOnlyTexture(data.shadow_map);
						switch (light.type)
						{
						case ELightType::Directional:
							if (light.use_cascades) shadow_cpu_handles[2] = shadow_map;
							else shadow_cpu_handles[0] = shadow_map;
							break;
						case ELightType::Spot:
							shadow_cpu_handles[0] = shadow_map;
							break;
						case ELightType::Point:
							shadow_cpu_handles[1] = shadow_map;
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
				}

				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

				if (light.volumetric)
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
					cmd_list->SetGraphicsRootConstantBufferView(3, global_data.postprocess_cbuffer_address);

					Descriptor cpu_handles[] = { context.GetReadOnlyTexture(data.depth), {} };
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
				}
			});

	}

}

