#include "MipsGenerator.h"
#include "d3dx12.h"
#include "PSOCache.h"
#include "../Graphics/GfxDevice.h"
#include "../Graphics/GfxTexture.h"
#include "../Graphics/GfxShaderCompiler.h"
#include "../Graphics/GfxCommandList.h"
#include "../Graphics/GfxPipelineState.h"
#include "../Graphics/GfxDescriptorAllocator.h"
#include "../Utilities/DWParam.h"


//https://slindev.com/d3d12-texture-mipmap-generation/

namespace adria
{

	MipsGenerator::MipsGenerator(GfxDevice* gfx, uint32 max_textures) : gfx(gfx)
	{
		CreateHeap(max_textures);
	}

	MipsGenerator::~MipsGenerator() {}

	void MipsGenerator::Add(GfxTexture* texture)
	{
		resources.push_back(texture);
	}

	void MipsGenerator::Generate(GfxCommandList* gfx_cmd_list)
	{
		if (resources.empty()) return;
		ID3D12Device* device = gfx->GetDevice();

		std::vector<GfxDescriptor> used_descriptors;

		auto command_list_native = gfx_cmd_list->GetNative();
		ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->GetHeap() };
		command_list_native->SetDescriptorHeaps(1, pp_heaps);
		command_list_native->SetComputeRootSignature(gfx->GetCommonRootSignature()); //is this needed?
		command_list_native->SetPipelineState(*PSOCache::Get(GfxPipelineStateID::GenerateMips));
		for (auto texture : resources)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc{};
			src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			D3D12_UNORDERED_ACCESS_VIEW_DESC dst_uav_desc{};
			dst_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			D3D12_RESOURCE_DESC tex_desc = texture->GetNative()->GetDesc();
			uint32 const mipmap_levels = tex_desc.MipLevels;

			uint32 i{};
			for (uint32 top_mip = 0; top_mip < mipmap_levels - 1; top_mip++)
			{
				uint32 dst_width = (std::max)((uint32)tex_desc.Width >> (top_mip + 1), 1u);
				uint32 dst_height = (std::max)(tex_desc.Height >> (top_mip + 1), 1u);

				GfxDescriptor handle1 = descriptor_allocator->AllocateDescriptor();
				src_srv_desc.Format = tex_desc.Format;
				src_srv_desc.Texture2D.MipLevels = 1;
				src_srv_desc.Texture2D.MostDetailedMip = top_mip;
				device->CreateShaderResourceView(texture->GetNative(), &src_srv_desc, handle1);

				GfxDescriptor handle2 = descriptor_allocator->AllocateDescriptor();
				dst_uav_desc.Format = tex_desc.Format;
				dst_uav_desc.Texture2D.MipSlice = top_mip + 1;
				device->CreateUnorderedAccessView(texture->GetNative(), nullptr, &dst_uav_desc, handle2);

				i = handle1.GetIndex();
				command_list_native->SetComputeRoot32BitConstant(1, DWParam(1.0f / dst_width).Uint, 0);
				command_list_native->SetComputeRoot32BitConstant(1, DWParam(1.0f / dst_height).Uint, 1);
				command_list_native->SetComputeRoot32BitConstant(1, i, 2);
				command_list_native->SetComputeRoot32BitConstant(1, i + 1, 3);

				command_list_native->Dispatch((std::max)(dst_width / 8u, 1u), (std::max)(dst_height / 8u, 1u), 1);

				auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture->GetNative());
				command_list_native->ResourceBarrier(1, &uav_barrier);

				used_descriptors.push_back(handle1);
				used_descriptors.push_back(handle2);
			}
		}
		resources.clear();

		for (auto& descriptor : used_descriptors) descriptor_allocator->FreeDescriptor(descriptor);
	}

	void MipsGenerator::CreateHeap(uint32 max_textures)
	{
		GfxDescriptorAllocatorDesc desc{};
		desc.descriptor_count = 20 * max_textures + 200;
		desc.shader_visible = true;
		desc.type = GfxDescriptorHeapType::CBV_SRV_UAV;
		descriptor_allocator = std::make_unique<GfxDescriptorAllocator>(gfx, desc);
	}

}

