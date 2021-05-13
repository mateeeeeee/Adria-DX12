#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <optional>
#include "d3dx12.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"


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

        RenderPass(render_pass_desc_t const& desc) : flags(desc.render_pass_flags),
            width(desc.width), height(desc.height)
        {
            for (auto const& attachment : desc.rtv_attachments)
            {
                D3D12_RENDER_PASS_RENDER_TARGET_DESC rtv_desc{};
                rtv_desc.cpuDescriptor = attachment.cpu_handle;
                rtv_desc.BeginningAccess = { attachment.beginning_access, attachment.clear_value };
                rtv_desc.EndingAccess = { attachment.ending_access, {} }; //resolve params are {}

                rtvs.push_back(std::move(rtv_desc));
           }

            if (desc.dsv_attachment)
            {
                auto const& _dsv_desc = desc.dsv_attachment.value();

                D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsv_desc{};
                dsv_desc.cpuDescriptor = _dsv_desc.cpu_handle;
                dsv_desc.DepthBeginningAccess = { _dsv_desc.depth_beginning_access, _dsv_desc.clear_value };
                dsv_desc.StencilBeginningAccess = { _dsv_desc.stencil_beginning_access, _dsv_desc.clear_value };
                dsv_desc.DepthEndingAccess = { _dsv_desc.depth_ending_access, {} };
                dsv_desc.StencilEndingAccess = { _dsv_desc.stencil_ending_access, {} };

                dsv = std::make_unique<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC>(dsv_desc);
            }
        }

        void Begin(ID3D12GraphicsCommandList4* cmd_list)
        {

            cmd_list->BeginRenderPass(static_cast<u32>(rtvs.size()), rtvs.data(), dsv.get(), flags);

            D3D12_VIEWPORT vp = {};
            vp.Width = (f32)width;
            vp.Height = (f32)height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            cmd_list->RSSetViewports(1, &vp);
            D3D12_RECT rect = {};
            rect.bottom = (i64)height;
            rect.left = 0;
            rect.right = (i64)width;
            rect.top = 0;
            cmd_list->RSSetScissorRects(1, &rect);

        }

        void End(ID3D12GraphicsCommandList4* cmd_list)
        {
            cmd_list->EndRenderPass();
        }

    private:
        std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> rtvs{};
        std::unique_ptr<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> dsv = nullptr;
        D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE;
        u32 width = 0;
        u32 height = 0;
    };

}