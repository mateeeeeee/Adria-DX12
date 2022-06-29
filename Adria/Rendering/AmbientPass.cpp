#include "AmbientPass.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	AmbientPass::AmbientPass(uint32 w, uint32 h)
		: width(w), height(h)
	{}

	void AmbientPass::AddPass(RenderGraph& rendergraph)
	{
		struct AmbientPassData
		{
			RGTextureReadOnlyId gbuffer_normal_srv;
			RGTextureReadOnlyId gbuffer_albedo_srv;
			RGTextureReadOnlyId gbuffer_emissive_srv;
			RGTextureReadOnlyId depth_stencil_srv;

			RGTextureReadOnlyId ambient_occlusion_srv;
			RGTextureReadOnlyId env_srv;
			RGTextureReadOnlyId irmap_srv;
			RGTextureReadOnlyId brdf_srv;
		};

		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<AmbientPassData>("Ambient Pass",
			[=](AmbientPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				
				builder.DeclareTexture(RG_RES_NAME(HDR_RenderTarget), render_target_desc);
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Clear_Preserve);
				data.gbuffer_normal_srv = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.gbuffer_albedo_srv = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_PixelShader);
				data.gbuffer_emissive_srv = builder.ReadTexture(RG_RES_NAME(GBufferEmissive), ReadAccess_PixelShader);
				data.depth_stencil_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				if (builder.IsTextureDeclared(RG_RES_NAME(AmbientOcclusion)))
				{
					data.ambient_occlusion_srv = builder.ReadTexture(RG_RES_NAME(AmbientOcclusion), ReadAccess_PixelShader);
				}
				if (builder.IsTextureDeclared(RG_RES_NAME(EnvTexture)) && 
					builder.IsTextureDeclared(RG_RES_NAME(IrmapTexture)) &&
					builder.IsTextureDeclared(RG_RES_NAME(BrdfTexture)))
				{
					data.env_srv = builder.ReadTexture(RG_RES_NAME(EnvTexture), ReadAccess_PixelShader);
					data.irmap_srv = builder.ReadTexture(RG_RES_NAME(IrmapTexture), ReadAccess_PixelShader);
					data.brdf_srv = builder.ReadTexture(RG_RES_NAME(BrdfTexture), ReadAccess_PixelShader);
				}

				builder.SetViewport(width, height);
			},
			[&](AmbientPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::AmbientPBR));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);

				bool has_ao = data.ambient_occlusion_srv.IsValid();
				bool has_ibl = data.env_srv.IsValid() && data.irmap_srv.IsValid() && data.brdf_srv.IsValid();
				if (has_ao && has_ibl) cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::AmbientPBR_AO_IBL));
				else if (has_ao && has_ibl) cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::AmbientPBR_AO));
				else if (!has_ao && has_ibl) cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::AmbientPBR_IBL));
				else cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::AmbientPBR));

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal_srv),
					context.GetReadOnlyTexture(data.gbuffer_albedo_srv), context.GetReadOnlyTexture(data.gbuffer_emissive_srv), context.GetReadOnlyTexture(data.depth_stencil_srv) };
				uint32 src_range_sizes[] = { 1,1,1,1 };
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, dst_descriptor);

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { global_data.null_srv_texture2d, global_data.null_srv_texturecube,
															   global_data.null_srv_texturecube, global_data.null_srv_texture2d };
				uint32 src_range_sizes2[] = { 1,1,1,1 };

				if (has_ao) cpu_handles2[0] = context.GetReadOnlyTexture(data.ambient_occlusion_srv);
				if (has_ibl)
				{
					cpu_handles2[1] = context.GetReadOnlyTexture(data.env_srv);
					cpu_handles2[2] = context.GetReadOnlyTexture(data.irmap_srv);
					cpu_handles2[3] = context.GetReadOnlyTexture(data.brdf_srv);
				}

				descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles2));
				dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				uint32 dst_range_sizes2[] = { (uint32)ARRAYSIZE(cpu_handles2) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes2, ARRAYSIZE(cpu_handles2), cpu_handles2, src_range_sizes2,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);

				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			});
	}

	void AmbientPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}