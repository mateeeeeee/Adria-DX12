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

	class VertexBuffer
	{
	public:

		template<typename vertex_t>
		VertexBuffer(GraphicsCoreDX12* gfx, vertex_t* vertices, size_t vertex_count)
		{

			auto allocator = gfx->GetAllocator();
			auto command_list = gfx->GetDefaultCommandList();

			D3D12_RESOURCE_DESC resource_desc = {};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Alignment = 0;
			resource_desc.Width = vertex_count * sizeof(vertex_t);
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.SampleDesc.Count = 1;
			resource_desc.SampleDesc.Quality = 0;
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			D3D12MA::ALLOCATION_DESC allocation_desc = {};
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;


			D3D12MA::Allocation* alloc;
			HRESULT hr = allocator->CreateResource(
				&allocation_desc,
				&resource_desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				&alloc,
				IID_PPV_ARGS(&vb));

			allocation.reset(alloc);

			vb->SetName(L"Vertex Buffer");

			D3D12MA::ALLOCATION_DESC vBufferUploadAllocDesc = {};
			vBufferUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			D3D12_RESOURCE_DESC vertexBufferUploadResourceDesc = {};
			vertexBufferUploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			vertexBufferUploadResourceDesc.Alignment = 0;
			vertexBufferUploadResourceDesc.Width = vertex_count * sizeof(vertex_t);
			vertexBufferUploadResourceDesc.Height = 1;
			vertexBufferUploadResourceDesc.DepthOrArraySize = 1;
			vertexBufferUploadResourceDesc.MipLevels = 1;
			vertexBufferUploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			vertexBufferUploadResourceDesc.SampleDesc.Count = 1;
			vertexBufferUploadResourceDesc.SampleDesc.Quality = 0;
			vertexBufferUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			vertexBufferUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			D3D12MA::Allocation* vBufferUploadHeapAllocation = nullptr;
			BREAK_IF_FAILED(allocator->CreateResource(
				&vBufferUploadAllocDesc,
				&vertexBufferUploadResourceDesc, // resource description for a buffer
				D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
				nullptr,
				&vBufferUploadHeapAllocation, __uuidof(nullptr), nullptr
			));

			// store vertex buffer in upload heap
			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData = reinterpret_cast<BYTE const*>(vertices); // pointer to our vertex array
			vertexData.RowPitch = vertex_count * sizeof(vertex_t); // size of all our triangle vertex data
			vertexData.SlicePitch = vertex_count * sizeof(vertex_t); // also the size of our triangle vertex data

			UINT64 r = UpdateSubresources(command_list, vb.Get(), vBufferUploadHeapAllocation->GetResource(), 0, 0, 1, &vertexData);

			D3D12_RESOURCE_BARRIER vbBarrier = {};
			vbBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			vbBarrier.Transition.pResource = vb.Get();
			vbBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			vbBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			vbBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			command_list->ResourceBarrier(1, &vbBarrier);

			// Initialize the vertex buffer view.
			vb_view.BufferLocation = vb->GetGPUVirtualAddress();
			vb_view.StrideInBytes = sizeof(vertex_t);
			vb_view.SizeInBytes = static_cast<UINT>(vertex_count) * sizeof(vertex_t);

			gfx->AddToReleaseQueue(vBufferUploadHeapAllocation);
		}

		template<typename vertex_t>
		VertexBuffer(GraphicsCoreDX12* gfx, std::vector<vertex_t> const& vertices)
			: VertexBuffer(gfx, vertices.data(), vertices.size())
		{
		}

		VertexBuffer(VertexBuffer const&) = delete;
		VertexBuffer(VertexBuffer&& o) noexcept = default;

		VertexBuffer& operator=(VertexBuffer const&) = delete;
		VertexBuffer& operator=(VertexBuffer&& o) noexcept = default;

		~VertexBuffer() = default;

		void Bind(ID3D12GraphicsCommandList* command_list, UINT slot = 0u) const
		{
			command_list->IASetVertexBuffers(slot, 1, &vb_view);
		}

		D3D12_VERTEX_BUFFER_VIEW View() const
		{
			return vb_view;
		}

		ID3D12Resource* Resource() const
		{
			return vb.Get();
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> vb;
		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vb_view;

	};

}