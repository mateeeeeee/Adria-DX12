#include "MipsGenerator.h"
#include "LinearGPUDescriptorAllocator.h"
#include "GfxShaderCompiler.h"
#include "d3dx12.h"
#include "../Utilities/DWParam.h"
#include "../Rendering/PSOCache.h"

//https://slindev.com/d3d12-texture-mipmap-generation/

namespace adria
{

	MipsGenerator::MipsGenerator(GfxDevice* gfx, UINT max_textures) : gfx(gfx)
	{
		CreateHeap(max_textures);
	}

	MipsGenerator::~MipsGenerator() {}

	void MipsGenerator::Add(ID3D12Resource* texture)
	{
		resources.push_back(texture);
	}

	void MipsGenerator::Generate(ID3D12GraphicsCommandList* command_list)
	{
		if (resources.empty()) return;
		ID3D12Device* device = gfx->GetDevice();

		ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
		command_list->SetDescriptorHeaps(1, pp_heaps);
		command_list->SetComputeRootSignature(gfx->GetCommonRootSignature()); //is this needed?
		command_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::GenerateMips));
		for (auto texture : resources)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc = {};
			src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			D3D12_UNORDERED_ACCESS_VIEW_DESC dst_uav_desc = {};
			dst_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();
			UINT const mipmap_levels = tex_desc.MipLevels;

			UINT i{};
			for (UINT top_mip = 0; top_mip < mipmap_levels - 1; top_mip++)
			{
				UINT dst_width = (std::max)((UINT)tex_desc.Width >> (top_mip + 1), 1u);
				UINT dst_height = (std::max)(tex_desc.Height >> (top_mip + 1), 1u);

				i = (UINT)descriptor_allocator->AllocateRange(2);
				DescriptorHandle handle1 = descriptor_allocator->GetHandle(i);
				src_srv_desc.Format = tex_desc.Format;
				src_srv_desc.Texture2D.MipLevels = 1;
				src_srv_desc.Texture2D.MostDetailedMip = top_mip;
				device->CreateShaderResourceView(texture, &src_srv_desc, handle1);

				DescriptorHandle handle2 = descriptor_allocator->GetHandle(i + 1);
				dst_uav_desc.Format = tex_desc.Format;
				dst_uav_desc.Texture2D.MipSlice = top_mip + 1;
				device->CreateUnorderedAccessView(texture, nullptr, &dst_uav_desc, handle2);

				command_list->SetComputeRoot32BitConstant(1, DWParam(1.0f / dst_width).Uint, 0);
				command_list->SetComputeRoot32BitConstant(1, DWParam(1.0f / dst_height).Uint, 1);
				command_list->SetComputeRoot32BitConstant(1, i, 2);
				command_list->SetComputeRoot32BitConstant(1, i + 1, 3);

				command_list->Dispatch((std::max)(dst_width / 8u, 1u), (std::max)(dst_height / 8u, 1u), 1);

				auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture);
				command_list->ResourceBarrier(1, &uav_barrier);
			}
		}
		resources.clear();
		descriptor_allocator->Clear();
	}

	void MipsGenerator::CreateHeap(UINT max_textures)
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc = {};
		shader_visible_desc.NumDescriptors = 20 * max_textures + 200; //approximate number of descriptors as : ~ max_textures * 2 * 10 (avg mip levels)
		shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptor_allocator = std::make_unique<LinearGPUDescriptorAllocator>(gfx->GetDevice(), shader_visible_desc);
	}

}

