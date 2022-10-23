#include "RenderPass.h"


namespace adria
{

	RenderPass::RenderPass(RenderPassDesc const& desc) : flags(desc.render_pass_flags),
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

	void RenderPass::Begin(ID3D12GraphicsCommandList4* cmd_list, bool legacy)
	{
		if (!legacy)
		{
			cmd_list->BeginRenderPass(static_cast<uint32>(rtvs.size()), rtvs.data(), dsv.get(), flags);
		}
		else
		{
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtv_handles{};
			D3D12_CPU_DESCRIPTOR_HANDLE* dsv_handle = nullptr;
			for (auto const& rtv : rtvs)
			{
				rtv_handles.push_back(rtv.cpuDescriptor);
				if(rtv.BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) 
					cmd_list->ClearRenderTargetView(rtv.cpuDescriptor, rtv.BeginningAccess.Clear.ClearValue.Color, 0, nullptr);
			}

			if (dsv != nullptr)
			{
				dsv_handle = &dsv->cpuDescriptor;
				if(dsv->DepthBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
					cmd_list->ClearDepthStencilView(dsv->cpuDescriptor, D3D12_CLEAR_FLAG_DEPTH, dsv->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth,
													dsv->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil, 0, nullptr);
			}
			cmd_list->OMSetRenderTargets((UINT)rtv_handles.size(), rtv_handles.data(), FALSE, dsv_handle);
		}

		D3D12_VIEWPORT vp = {};
		vp.Width = (float)width;
		vp.Height = (float)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		cmd_list->RSSetViewports(1, &vp);
		D3D12_RECT rect = {};
		rect.bottom = (int64)height;
		rect.left = 0;
		rect.right = (int64)width;
		rect.top = 0;
		cmd_list->RSSetScissorRects(1, &rect);
	}

	void RenderPass::End(ID3D12GraphicsCommandList4* cmd_list, bool legacy)
	{
		if (!legacy) cmd_list->EndRenderPass();
	}

}

