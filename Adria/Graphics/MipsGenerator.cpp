#include "MipsGenerator.h"
#include "LinearOnlineDescriptorAllocator.h"
#include "DWParam.h"
#include "ShaderUtility.h"
#include "d3dx12.h"
#include "../Rendering/RootSigPSOManager.h"

//https://slindev.com/d3d12-texture-mipmap-generation/

namespace adria
{

	MipsGenerator::MipsGenerator(ID3D12Device* device, UINT max_textures) : device(device)
	{
		CreateHeap(max_textures);
	}

	void MipsGenerator::Add(ID3D12Resource* texture)
	{
		resources.push_back(texture);
	}

	void MipsGenerator::Generate(ID3D12GraphicsCommandList* command_list)
	{
		ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
		command_list->SetDescriptorHeaps(1, pp_heaps);

		//Set root signature, pso and descriptor heap
		command_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::GenerateMips));
		command_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::GenerateMips));

		for (auto texture : resources)
		{
			//Prepare the shader resource view description for the source texture
			D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc = {};
			src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			//Prepare the unordered access view description for the destination texture
			D3D12_UNORDERED_ACCESS_VIEW_DESC dst_uav_desc = {};
			dst_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();
			UINT const mipmap_levels = tex_desc.MipLevels;

			auto transition_barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			command_list->ResourceBarrier(1, &transition_barrier);

			OffsetType i{};
			for (UINT top_mip = 0; top_mip < mipmap_levels - 1; top_mip++)
			{
				//Get mipmap dimensions
				UINT dst_width = (std::max)((UINT)tex_desc.Width >> (top_mip + 1), 1u);
				UINT dst_height = (std::max)(tex_desc.Height >> (top_mip + 1), 1u);

				i = descriptor_allocator->AllocateRange(2);
				DescriptorHandle handle1 = descriptor_allocator->GetHandle(i);

				src_srv_desc.Format = tex_desc.Format;
				src_srv_desc.Texture2D.MipLevels = 1;
				src_srv_desc.Texture2D.MostDetailedMip = top_mip;
				device->CreateShaderResourceView(texture, &src_srv_desc, handle1);

				DescriptorHandle handle2 = descriptor_allocator->GetHandle(i + 1);
				dst_uav_desc.Format = tex_desc.Format;
				dst_uav_desc.Texture2D.MipSlice = top_mip + 1;
				device->CreateUnorderedAccessView(texture, nullptr, &dst_uav_desc, handle2);
				//Pass the destination texture pixel size to the shader as constants
				command_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_width).Uint, 0);
				command_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_height).Uint, 1);
				command_list->SetComputeRootDescriptorTable(1, handle1);
				command_list->SetComputeRootDescriptorTable(2, handle2);

				//Dispatch the compute shader with one thread per 8x8 pixels
				command_list->Dispatch((std::max)(dst_width / 8u, 1u), (std::max)(dst_height / 8u, 1u), 1);

				//Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
				auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture);
				command_list->ResourceBarrier(1, &uav_barrier);
			}

			transition_barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			command_list->ResourceBarrier(1, &transition_barrier);
		}

		resources.clear();
		descriptor_allocator->Clear();
	}

	void MipsGenerator::CreateHeap(UINT max_textures) //approximate number of descriptors as : ~ max_textures * 2 * 10 (avg mip levels)
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc = {};
		shader_visible_desc.NumDescriptors = 20 * max_textures + 200;
		shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		descriptor_allocator = std::make_unique<LinearOnlineDescriptorAllocator>(device, shader_visible_desc);
	}

}

