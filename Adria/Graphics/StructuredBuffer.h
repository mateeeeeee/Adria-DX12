#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include "d3dx12.h"
#include "Releasable.h"
#include "GraphicsCoreDX12.h"
#include "../Core/Macros.h"


namespace adria
{

	
	template<typename T>
	class StructuredBuffer
	{
	public:

		StructuredBuffer(ID3D12Device* device, U32 element_count, bool counter = false, D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			: device(device), current_state(initial_state), element_count(element_count)
		{
			D3D12_RESOURCE_DESC resource_desc{};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Width = element_count * sizeof(T);
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			resource_desc.SampleDesc.Count = 1;
			resource_desc.SampleDesc.Quality = 0;
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Alignment = 0;

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			BREAK_IF_FAILED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
				current_state, nullptr, IID_PPV_ARGS(&buffer)));

			if (counter)
			{
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				resource_desc.Width = sizeof(U32);
				resource_desc.Height = 1;
				resource_desc.DepthOrArraySize = 1;
				resource_desc.MipLevels = 1;
				resource_desc.Format = DXGI_FORMAT_UNKNOWN;
				resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				resource_desc.SampleDesc.Count = 1;
				resource_desc.SampleDesc.Quality = 0;
				resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				resource_desc.Alignment = 0;

				BREAK_IF_FAILED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&counter_buffer)));
			}
		}
		StructuredBuffer(StructuredBuffer const&) = delete;
		StructuredBuffer(StructuredBuffer&&) = delete;

		~StructuredBuffer() = default;

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements = element_count;
			srv_desc.Buffer.StructureByteStride = sizeof(T);
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			device->CreateShaderResourceView(buffer.Get(), &srv_desc, handle);

			srv_handle = handle;
			
		}

		D3D12_CPU_DESCRIPTOR_HANDLE SRV() const
		{
			return srv_handle;
		}

		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = element_count;
			uav_desc.Buffer.StructureByteStride = sizeof(T);
			uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			uav_desc.Buffer.CounterOffsetInBytes = 0;

			device->CreateUnorderedAccessView(buffer.Get(), counter_buffer.Get(), &uav_desc, handle);

			uav_handle = handle;
		}

		void CreateCounterUAV(D3D12_CPU_DESCRIPTOR_HANDLE counter_handle)
		{
			if (counter_buffer)
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uav_desc.Format = DXGI_FORMAT_UNKNOWN;
				uav_desc.Buffer.CounterOffsetInBytes = 0;
				uav_desc.Buffer.FirstElement = 0;
				uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				uav_desc.Buffer.NumElements = 1;
				uav_desc.Buffer.StructureByteStride = sizeof(U32);
				device->CreateUnorderedAccessView(counter_buffer.Get(), nullptr, &uav_desc, counter_handle);

				uav_counter_handle = counter_handle;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE UAV() const
		{
			return uav_handle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE CounterUAV() const
		{
			return uav_counter_handle;
			CD3DX12_DESCRIPTOR_RANGE  d;
		}

		ID3D12Resource* Buffer() const
		{
			return buffer.Get();
		}

		ID3D12Resource* CounterBuffer() const
		{
			return counter_buffer.Get();
		}

	private:
		ID3D12Device* device;
		Microsoft::WRL::ComPtr<ID3D12Resource> buffer = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> counter_buffer = nullptr;
		U32 const element_count;
		D3D12_RESOURCE_STATES current_state;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_handle{};
		D3D12_CPU_DESCRIPTOR_HANDLE uav_handle{};
		D3D12_CPU_DESCRIPTOR_HANDLE uav_counter_handle{};
	};

}