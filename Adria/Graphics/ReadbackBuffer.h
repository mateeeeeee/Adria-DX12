#pragma once
#include "GraphicsCoreDX12.h"
#include "d3dx12.h"
#include <memory>

namespace adria
{
	class ReadbackBuffer
	{
	public:

		ReadbackBuffer(ID3D12Device* device, UINT64 size) : size{ size }
		{
			ADRIA_ASSERT(size > 0);
			
			D3D12_RESOURCE_DESC resource_desc{};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Width = UINT(size);
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			resource_desc.SampleDesc.Count = 1;
			resource_desc.SampleDesc.Quality = 0;
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Alignment = 0;

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
			BREAK_IF_FAILED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)));
		}
		~ReadbackBuffer() = default;

		void const* Map() const
		{
			ADRIA_ASSERT(resource != nullptr);
			void* data = nullptr;
			resource->Map(0, nullptr, &data);
			return data;
		}

		template<typename T> 
		T const* Map() const { return reinterpret_cast<const T*>(Map()); };

		void Unmap() const
		{
			ADRIA_ASSERT(resource != nullptr);
			resource->Unmap(0, nullptr);
		}

		ID3D12Resource* Resource() const 
		{
			return resource.Get();
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
		UINT64 size;
	};
}