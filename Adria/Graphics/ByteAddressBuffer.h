#pragma once
#include "GraphicsDeviceDX12.h"
#include "d3dx12.h"
#include <memory>


namespace adria
{
	class ByteAddressBuffer
	{
	public:
		ByteAddressBuffer(ID3D12Device* device, UINT64 element_count) : device{ device }, element_count{ element_count }, element_stride{ 4 }
		{
			ADRIA_ASSERT(element_count > 0);

			D3D12_RESOURCE_DESC resource_desc{};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Width = UINT(element_count) * element_stride;
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
				D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));
		}

		template<typename T>
		ByteAddressBuffer(GraphicsDevice* gfx, T const* data, size_t element_count, D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			: device(gfx->GetDevice()), element_count(element_count), element_stride{sizeof(T)}
		{
			auto command_list = gfx->GetDefaultCommandList();

			D3D12_RESOURCE_DESC byteaddress_buffer_resource_desc{};
			byteaddress_buffer_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			byteaddress_buffer_resource_desc.Width = element_count * element_stride;
			byteaddress_buffer_resource_desc.Height = 1;
			byteaddress_buffer_resource_desc.DepthOrArraySize = 1;
			byteaddress_buffer_resource_desc.MipLevels = 1;
			byteaddress_buffer_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			byteaddress_buffer_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			byteaddress_buffer_resource_desc.SampleDesc.Count = 1;
			byteaddress_buffer_resource_desc.SampleDesc.Quality = 0;
			byteaddress_buffer_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			byteaddress_buffer_resource_desc.Alignment = 0;

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			BREAK_IF_FAILED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, 
				&byteaddress_buffer_resource_desc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&buffer)));

			ID3D12Resource* upload_buffer = nullptr;
			D3D12_RESOURCE_DESC byteaddress_buffer_upload_resource_desc{};
			byteaddress_buffer_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			byteaddress_buffer_upload_resource_desc.Alignment = 0;
			byteaddress_buffer_upload_resource_desc.Width = element_count * sizeof(T);
			byteaddress_buffer_upload_resource_desc.Height = 1;
			byteaddress_buffer_upload_resource_desc.DepthOrArraySize = 1;
			byteaddress_buffer_upload_resource_desc.MipLevels = 1;
			byteaddress_buffer_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			byteaddress_buffer_upload_resource_desc.SampleDesc.Count = 1;
			byteaddress_buffer_upload_resource_desc.SampleDesc.Quality = 0;
			byteaddress_buffer_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			byteaddress_buffer_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			BREAK_IF_FAILED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &byteaddress_buffer_upload_resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_buffer)));

			// store vertex buffer in upload heap
			D3D12_SUBRESOURCE_DATA structure_data{};
			structure_data.pData = data;
			structure_data.RowPitch = element_count * sizeof(T);
			structure_data.SlicePitch = element_count * sizeof(T);

			UINT64 r = UpdateSubresources(command_list, buffer.Get(), upload_buffer, 0, 0, 1, &structure_data);
			ADRIA_ASSERT(r > 0);

			D3D12_RESOURCE_BARRIER sb_barrier = {};
			sb_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			sb_barrier.Transition.pResource = buffer.Get();
			sb_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			sb_barrier.Transition.StateAfter = initial_state;
			sb_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			command_list->ResourceBarrier(1, &sb_barrier);

			gfx->AddToReleaseQueue(upload_buffer);
		}

		~ByteAddressBuffer() = default;

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Buffer.NumElements = element_count * element_stride / 4;
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			device->CreateShaderResourceView(buffer.Get(), &srv_desc, handle);
			srv_handle = handle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE SRV() const
		{
			return srv_handle;
		}

		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			uav_desc.Buffer.NumElements = element_count;
			uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

			device->CreateUnorderedAccessView(buffer.Get(), nullptr, &uav_desc, handle);
			uav_handle = handle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE UAV() const
		{
			return uav_handle;
		}

		ID3D12Resource* Resource() const
		{
			return buffer.Get();
		}

	private:
		ID3D12Device* device;
		Microsoft::WRL::ComPtr<ID3D12Resource> buffer = nullptr;
		UINT64 const element_count;
		UINT64 const element_stride;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_handle{};
		D3D12_CPU_DESCRIPTOR_HANDLE uav_handle{};
	};
}