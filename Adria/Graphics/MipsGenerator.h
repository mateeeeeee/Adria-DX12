#pragma once
#include "DWParam.h"
#include "ShaderUtility.h"
#include "d3dx12.h"
#include "GraphicsCoreDX12.h"
#include "LinearDescriptorAllocator.h"

//https://slindev.com/d3d12-texture-mipmap-generation/


namespace adria
{

	class MipsGenerator
	{
	public:

		MipsGenerator(ID3D12Device* device, UINT max_textures) : device(device)
		{
			CreateRootSignature();

			CreatePSO();

			CreateHeap(max_textures);
		}

		void Add(ID3D12Resource* texture)
		{
			resources.push_back(texture);
		}

		void Generate(ID3D12GraphicsCommandList* command_list)
		{

			ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
			command_list->SetDescriptorHeaps(1, pp_heaps);

			//Set root signature, pso and descriptor heap
			command_list->SetComputeRootSignature(root_signature.Get());
			command_list->SetPipelineState(pso.Get());

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
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle1 = descriptor_allocator->GetCpuHandle(i);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle1 = descriptor_allocator->GetGpuHandle(i);

					src_srv_desc.Format = tex_desc.Format;
					src_srv_desc.Texture2D.MipLevels = 1;
					src_srv_desc.Texture2D.MostDetailedMip = top_mip;
					device->CreateShaderResourceView(texture, &src_srv_desc, cpu_handle1);


					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle2 = descriptor_allocator->GetCpuHandle(i + 1);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle2 = descriptor_allocator->GetGpuHandle(i + 1);
					dst_uav_desc.Format = tex_desc.Format;
					dst_uav_desc.Texture2D.MipSlice = top_mip + 1;
					device->CreateUnorderedAccessView(texture, nullptr, &dst_uav_desc, cpu_handle2);
					//Pass the destination texture pixel size to the shader as constants
					command_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_width).Uint, 0);
					command_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_height).Uint, 1);

					command_list->SetComputeRootDescriptorTable(1, gpu_handle1);
					command_list->SetComputeRootDescriptorTable(2, gpu_handle2);

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

	private:
		ID3D12Device* device;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
		std::unique_ptr<LinearDescriptorAllocator> descriptor_allocator;
		std::vector<ID3D12Resource*> resources;
	private:

		void CreateRootSignature()
		{
			
			D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};

			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
			{
				feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 srv_uav_ranges[2] = {};
			CD3DX12_ROOT_PARAMETER1 root_parameters[3] = {};
			srv_uav_ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
			srv_uav_ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
			root_parameters[0].InitAsConstants(2, 0);
			root_parameters[1].InitAsDescriptorTable(1, &srv_uav_ranges[0]);
			root_parameters[2].InitAsDescriptorTable(1, &srv_uav_ranges[1]);

			D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.MipLODBias = 0.0f;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.MaxAnisotropy = 0;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
			sampler.ShaderRegister = 0;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
			root_signature_desc.Init_1_1(_countof(root_parameters), root_parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;

			BREAK_IF_FAILED(D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error));
			BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));


		}

		void CreatePSO()
		{
			ShaderBlob cs_blob;

			ShaderInfo info{};
			info.shadersource = "Resources\\Shaders\\Misc\\GenerateMipsCS.hlsl";
			info.stage = ShaderStage::CS;

			ShaderUtility::CompileShader(info, cs_blob);
			//Create pipeline state object for the compute shader using the root signature.
			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = root_signature.Get();
			pso_desc.CS = cs_blob;
			device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso));
		}

		void CreateHeap(UINT max_textures) //approximate number of descriptors as : ~ max_textures * 2 * 10 (avg mip levels)
		{
			// Create copy queue:
			D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc = {};
			shader_visible_desc.NumDescriptors = 20 * max_textures + 200;
			shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

			//new LinearDescriptorAllocator(device.Get(), shader_visible_desc)
			descriptor_allocator = std::make_unique<LinearDescriptorAllocator>(device, shader_visible_desc);
		}

	};
}