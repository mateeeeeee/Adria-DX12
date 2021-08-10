#pragma once
#include <d3d12.h>
#include <optional>
#include "d3dx12.h"
#include "../Core/Definitions.h"



namespace adria
{

    struct rtv_attachment_desc_t
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE beginning_access;
        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE ending_access;
        D3D12_CLEAR_VALUE clear_value; 
    };

    struct dsv_attachment_desc_t
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE depth_beginning_access;
        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE    depth_ending_access;
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE    stencil_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE    stencil_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
        D3D12_CLEAR_VALUE clear_value;
    };

    struct render_pass_desc_t
    {
        std::vector<rtv_attachment_desc_t> rtv_attachments{};
        std::optional<dsv_attachment_desc_t> dsv_attachment = std::nullopt;
        D3D12_RENDER_PASS_FLAGS render_pass_flags = D3D12_RENDER_PASS_FLAG_NONE;
        u32 width;
        u32 height;
    };

    class RenderPass
    {
    public:

        RenderPass() = default;

        RenderPass(render_pass_desc_t const& desc);

        void Begin(ID3D12GraphicsCommandList4* cmd_list);

        void End(ID3D12GraphicsCommandList4* cmd_list);

    private:
        std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> rtvs{};
        std::unique_ptr<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> dsv = nullptr;
        D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE;
        u32 width = 0;
        u32 height = 0;
    };

}