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
            index_t* indices, size_t index_count) : index_count{ static_cast<UINT>(index_count) }
        {

            auto allocator = gfx->GetAllocator();
            auto command_list = gfx->GetDefaultCommandList();

            size_t ib_byte_size = sizeof(index_t) * index_count;
            
            // create default heap to hold index buffer
            D3D12MA::ALLOCATION_DESC indexBufferAllocDesc = {};
            indexBufferAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC indexBufferResourceDesc = {};
            indexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            indexBufferResourceDesc.Alignment = 0;
            indexBufferResourceDesc.Width = ib_byte_size;
            indexBufferResourceDesc.Height = 1;
            indexBufferResourceDesc.DepthOrArraySize = 1;
            indexBufferResourceDesc.MipLevels = 1;
            indexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            indexBufferResourceDesc.SampleDesc.Count = 1;
            indexBufferResourceDesc.SampleDesc.Quality = 0;
            indexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            indexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* alloc;
            BREAK_IF_FAILED(allocator->CreateResource(
                &indexBufferAllocDesc,
                &indexBufferResourceDesc, // resource description for a buffer
                D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
                nullptr, // optimized clear value must be null for this type of resource
                &alloc,
                IID_PPV_ARGS(&ib)));

            allocation.reset(alloc);
            // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
            ib->SetName(L"Index Buffer");

            // create upload heap to upload index buffer
            D3D12MA::ALLOCATION_DESC iBufferUploadAllocDesc = {};
            iBufferUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC indexBufferUploadResourceDesc = {};
            indexBufferUploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            indexBufferUploadResourceDesc.Alignment = 0;
            indexBufferUploadResourceDesc.Width = ib_byte_size;
            indexBufferUploadResourceDesc.Height = 1;
            indexBufferUploadResourceDesc.DepthOrArraySize = 1;
            indexBufferUploadResourceDesc.MipLevels = 1;
            indexBufferUploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            indexBufferUploadResourceDesc.SampleDesc.Count = 1;
            indexBufferUploadResourceDesc.SampleDesc.Quality = 0;
            indexBufferUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            indexBufferUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* iBufferUploadHeapAllocation = nullptr;
            BREAK_IF_FAILED(allocator->CreateResource(
                &iBufferUploadAllocDesc,
                &indexBufferUploadResourceDesc, // resource description for a buffer
                D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
                nullptr,
                &iBufferUploadHeapAllocation,
                __uuidof(nullptr), nullptr));

            // store vertex buffer in upload heap
            D3D12_SUBRESOURCE_DATA indexData = {};
            indexData.pData = indices;              // pointer to our index array
            indexData.RowPitch = ib_byte_size;      // size of all our index buffer
            indexData.SlicePitch = ib_byte_size;    // also the size of our index buffer

            // we are now creating a command with the command list to copy the data from
            // the upload heap to the default heap
            UINT64 r = UpdateSubresources(command_list, ib.Get(), iBufferUploadHeapAllocation->GetResource(), 0, 0, 1, &indexData);
            assert(r);

            // transition the index buffer data from copy destination state to vertex buffer state
            D3D12_RESOURCE_BARRIER ibBarrier = {};
            ibBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            ibBarrier.Transition.pResource = ib.Get();
            ibBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            ibBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            ibBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            command_list->ResourceBarrier(1, &ibBarrier);

            // Initialize the index buffer view.
            ib_view.BufferLocation = ib->GetGPUVirtualAddress();

            if (std::is_same_v<std::decay_t<index_t>, uint16_t>) ib_view.Format = DXGI_FORMAT_R16_UINT;
            else if (std::is_same_v<std::decay_t<index_t>, uint32_t>) ib_view.Format = DXGI_FORMAT_R32_UINT;
            else ib_view.Format = DXGI_FORMAT_UNKNOWN;
            ib_view.SizeInBytes = static_cast<UINT>(ib_byte_size);

            gfx->AddToReleaseQueue(iBufferUploadHeapAllocation);
        }

        template<typename index_t>
        IndexBuffer(GraphicsCoreDX12* gfx,
            std::vector<index_t> const& indices)
            : IndexBuffer(gfx, indices.data(), indices.size())
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

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> ib;
        ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
        D3D12_INDEX_BUFFER_VIEW ib_view;
        uint32 index_count;
    };

}
