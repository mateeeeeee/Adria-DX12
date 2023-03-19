#include "GenerateMipsPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../Graphics/RingGPUDescriptorAllocator.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Utilities/DWParam.h"

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
			[=](GenerateMipsPassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				GfxTexture const& texture = context.GetTexture(data.texture_src.GetResourceId());
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::GenerateMips));

				D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc{};
				src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

				D3D12_UNORDERED_ACCESS_VIEW_DESC dst_uav_desc{};
				dst_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

				GfxTextureDesc tex_desc = texture.GetDesc();
				UINT const mipmap_levels = tex_desc.mip_levels;

				for (UINT top_mip = 0; top_mip < mipmap_levels - 1; top_mip++)
				{
					UINT dst_width = (std::max)((UINT)tex_desc.width >> (top_mip + 1), 1u);
					UINT dst_height = (std::max)(tex_desc.height >> (top_mip + 1), 1u);

					UINT i = (UINT)descriptor_allocator->AllocateRange(2);

					DescriptorHandle handle1 = descriptor_allocator->GetHandle(i);
					src_srv_desc.Format = ConvertGfxFormat(tex_desc.format);
					src_srv_desc.Texture2D.MipLevels = 1;
					src_srv_desc.Texture2D.MostDetailedMip = top_mip;
					device->CreateShaderResourceView(texture.GetNative(), &src_srv_desc, handle1);

					DescriptorHandle handle2 = descriptor_allocator->GetHandle(i + 1);
					dst_uav_desc.Format = ConvertGfxFormat(tex_desc.format);
					dst_uav_desc.Texture2D.MipSlice = top_mip + 1;
					device->CreateUnorderedAccessView(texture.GetNative(), nullptr, &dst_uav_desc, handle2);
					
					cmd_list->SetComputeRoot32BitConstant(1, DWParam(1.0f / dst_width).Uint, 0);
					cmd_list->SetComputeRoot32BitConstant(1, DWParam(1.0f / dst_height).Uint, 1);
					cmd_list->SetComputeRoot32BitConstant(1, i, 2);
					cmd_list->SetComputeRoot32BitConstant(1, i + 1, 3);

					cmd_list->Dispatch((UINT)std::max(std::ceil(dst_width / 8.0f), 1.0f), (UINT)std::max(std::ceil(dst_height / 8.0f), 1.0f), 1);

					auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture.GetNative());
					cmd_list->ResourceBarrier(1, &uav_barrier);
				}
			}, RGPassType::Compute, RGPassFlags::None);
	}

}

