#pragma once
#include <d3d12.h>
#include <optional>
#include "d3dx12.h"
#include "../Core/Definitions.h"



namespace adria
{

    struct RtvAttachmentDesc
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE beginning_access;
        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE ending_access;
        D3D12_CLEAR_VALUE clear_value; 
    };

    struct DsvAttachmentDesc
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE depth_beginning_access;
        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE    depth_ending_access;
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE    stencil_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE    stencil_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
        D3D12_CLEAR_VALUE clear_value;
    };

    struct GfxRenderPassDesc
    {
        std::vector<RtvAttachmentDesc> rtv_attachments{};
        std::optional<DsvAttachmentDesc> dsv_attachment = std::nullopt;
        D3D12_RENDER_PASS_FLAGS render_pass_flags = D3D12_RENDER_PASS_FLAG_NONE;
        uint32 width;
        uint32 height;
    };

    class GfxRenderPass
    {
    public:

        GfxRenderPass() = default;

        explicit GfxRenderPass(GfxRenderPassDesc const& desc);

        void Begin(ID3D12GraphicsCommandList4* cmd_list, bool legacy = false);
        void End(ID3D12GraphicsCommandList4* cmd_list, bool legacy = false);

    private:
        std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> rtvs{};
        std::unique_ptr<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> dsv = nullptr;
        D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE;
        uint32 width = 0;
        uint32 height = 0;
    };
}