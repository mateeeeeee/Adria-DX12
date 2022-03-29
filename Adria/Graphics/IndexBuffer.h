#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include "d3dx12.h"
#include "GraphicsCoreDX12.h"
#include "Releasable.h"
#include "../Core/Macros.h"


namespace adria
{

    class IndexBuffer
    {
    public:
        template<typename index_t>
        IndexBuffer(GraphicsCoreDX12* gfx,
            index_t* indices, size_t index_count, bool used_in_rt = false) : index_count{ static_cast<UINT>(index_count) }
        {

            auto allocator = gfx->GetAllocator();
            auto command_list = gfx->GetDefaultCommandList();

            size_t ib_byte_size = sizeof(index_t) * index_count;
            
            // create default heap to hold index buffer
            D3D12MA::ALLOCATION_DESC index_buffer_alloc_desc{};
            index_buffer_alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC index_buffer_resource_desc{};
            index_buffer_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            index_buffer_resource_desc.Alignment = 0;
            index_buffer_resource_desc.Width = ib_byte_size;
            index_buffer_resource_desc.Height = 1;
            index_buffer_resource_desc.DepthOrArraySize = 1;
            index_buffer_resource_desc.MipLevels = 1;
            index_buffer_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
            index_buffer_resource_desc.SampleDesc.Count = 1;
            index_buffer_resource_desc.SampleDesc.Quality = 0;
            index_buffer_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            index_buffer_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* alloc;
            BREAK_IF_FAILED(allocator->CreateResource(
                &index_buffer_alloc_desc,
                &index_buffer_resource_desc, // resource description for a buffer
                D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
                nullptr, // optimized clear value must be null for this type of resource
                &alloc,
                IID_PPV_ARGS(&ib)));

            allocation.reset(alloc);
            // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
            ib->SetName(L"Index Buffer");

            // create upload heap to upload index buffer
            D3D12MA::ALLOCATION_DESC index_buffer_upload_alloc_desc = {};
            index_buffer_upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC index_buffer_upload_resource_desc = {};
            index_buffer_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            index_buffer_upload_resource_desc.Alignment = 0;
            index_buffer_upload_resource_desc.Width = ib_byte_size;
            index_buffer_upload_resource_desc.Height = 1;
            index_buffer_upload_resource_desc.DepthOrArraySize = 1;
            index_buffer_upload_resource_desc.MipLevels = 1;
            index_buffer_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
            index_buffer_upload_resource_desc.SampleDesc.Count = 1;
            index_buffer_upload_resource_desc.SampleDesc.Quality = 0;
            index_buffer_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            index_buffer_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* index_buffer_upload_heap_allocation = nullptr;
            BREAK_IF_FAILED(allocator->CreateResource(
                &index_buffer_upload_alloc_desc,
                &index_buffer_upload_resource_desc, // resource description for a buffer
                D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
                nullptr,
                &index_buffer_upload_heap_allocation,
                __uuidof(nullptr), nullptr));

            // store vertex buffer in upload heap
            D3D12_SUBRESOURCE_DATA index_data{};
            index_data.pData = indices;              // pointer to our index array
            index_data.RowPitch = ib_byte_size;      // size of all our index buffer
            index_data.SlicePitch = ib_byte_size;    // also the size of our index buffer

            UINT64 r = UpdateSubresources(command_list, ib.Get(), index_buffer_upload_heap_allocation->GetResource(), 0, 0, 1, &index_data);
            assert(r);

            // transition the index buffer data from copy destination state to vertex buffer state
            D3D12_RESOURCE_BARRIER ib_barrier{};
            ib_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            ib_barrier.Transition.pResource = ib.Get();
            ib_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            ib_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            if (used_in_rt) ib_barrier.Transition.StateAfter |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            ib_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            command_list->ResourceBarrier(1, &ib_barrier);

            // Initialize the index buffer view.
            ib_view.BufferLocation = ib->GetGPUVirtualAddress();
            if (std::is_same_v<std::decay_t<index_t>, uint16_t>) ib_view.Format = DXGI_FORMAT_R16_UINT;
            else if (std::is_same_v<std::decay_t<index_t>, uint32_t>) ib_view.Format = DXGI_FORMAT_R32_UINT;
            else ib_view.Format = DXGI_FORMAT_UNKNOWN;
            ib_view.SizeInBytes = static_cast<UINT>(ib_byte_size);

            gfx->AddToReleaseQueue(index_buffer_upload_heap_allocation);
        }

        template<typename index_t>
        IndexBuffer(GraphicsCoreDX12* gfx,
            std::vector<index_t> const& indices, bool used_in_rt = false)
            : IndexBuffer(gfx, indices.data(), indices.size(), used_in_rt)
        {
        }

        IndexBuffer(IndexBuffer const&) = delete;
        IndexBuffer(IndexBuffer&& o) noexcept = default;

        IndexBuffer& operator=(IndexBuffer const&) = delete;
        IndexBuffer& operator=(IndexBuffer&& o) noexcept = default;

        ~IndexBuffer() = default;

        void Bind(ID3D12GraphicsCommandList* command_list) const
        {
            command_list->IASetIndexBuffer(&ib_view);
        }

        D3D12_INDEX_BUFFER_VIEW View() const
        {
            return ib_view;
        }

        UINT IndexCount() const
        {
            return index_count;
        }

        ID3D12Resource* Resource() const
        {
            return ib.Get();
        }

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> ib;
        ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
        D3D12_INDEX_BUFFER_VIEW ib_view;
        uint32 index_count;
    };

}
