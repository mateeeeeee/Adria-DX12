#include "GenerateMipsPass.h"
#include "GlobalBlackboardData.h"
#include "PipelineState.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/DWParam.h"

namespace adria
{

	void GenerateMipsPass::AddPass(RenderGraph& rendergraph, RGResourceName texture_name)
	{
		struct GenerateMipsPassData
		{
			RGTextureReadOnlyId texture_src;
		};

		rendergraph.AddPass<GenerateMipsPassData>("GenerateMips Pass",
			[=](GenerateMipsPassData& data, RenderGraphBuilder& builder)
			{
				data.texture_src = builder.ReadTexture(texture_name, ReadAccess_NonPixelShader);
				builder.DummyWriteTexture(texture_name);
				builder.SetViewport(width, height);
			},
			[=](GenerateMipsPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				Texture const& texture = context.GetTexture(data.texture_src.GetResourceId());
				//Set root signature, pso and descriptor heap
				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::GenerateMips));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineState::GenerateMips));

				//Prepare the shader resource view description for the source texture
				D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc{};
				src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

				//Prepare the unordered access view description for the destination texture
				D3D12_UNORDERED_ACCESS_VIEW_DESC dst_uav_desc{};
				dst_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

				TextureDesc tex_desc = texture.GetDesc();
				UINT const mipmap_levels = tex_desc.mip_levels;

				OffsetType i{};
				for (UINT top_mip = 0; top_mip < mipmap_levels - 1; top_mip++)
				{
					//Get mipmap dimensions
					UINT dst_width = (std::max)((UINT)tex_desc.width >> (top_mip + 1), 1u);
					UINT dst_height = (std::max)(tex_desc.height >> (top_mip + 1), 1u);

					i = descriptor_allocator->AllocateRange(2);
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle1 = descriptor_allocator->GetHandle(i);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle1 = descriptor_allocator->GetHandle(i);

					src_srv_desc.Format = tex_desc.format;
					src_srv_desc.Texture2D.MipLevels = 1;
					src_srv_desc.Texture2D.MostDetailedMip = top_mip;
					device->CreateShaderResourceView(texture.GetNative(), &src_srv_desc, cpu_handle1);

					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle2 = descriptor_allocator->GetHandle(i + 1);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle2 = descriptor_allocator->GetHandle(i + 1);
					dst_uav_desc.Format = tex_desc.format;
					dst_uav_desc.Texture2D.MipSlice = top_mip + 1;
					device->CreateUnorderedAccessView(texture.GetNative(), nullptr, &dst_uav_desc, cpu_handle2);
					//Pass the destination texture pixel size to the shader as constants
					cmd_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_width).Uint, 0);
					cmd_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_height).Uint, 1);

					cmd_list->SetComputeRootDescriptorTable(1, gpu_handle1);
					cmd_list->SetComputeRootDescriptorTable(2, gpu_handle2);

					//Dispatch the compute shader with one thread per 8x8 pixels
					cmd_list->Dispatch((UINT)std::max(std::ceil(dst_width / 8.0f), 1.0f), (UINT)std::max(std::ceil(dst_height / 8.0f), 1.0f), 1);

					//Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
					auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture.GetNative());
					cmd_list->ResourceBarrier(1, &uav_barrier);
				}
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

}

