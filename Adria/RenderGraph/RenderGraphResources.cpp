#include "RenderGraphResources.h"
#include "RenderGraph.h"

namespace adria
{

	RenderGraphResources::RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass) : rg(rg), rg_pass(rg_pass)
	{}

	RGResource& RenderGraphResources::GetResource(RGResourceHandle handle)
	{
		return *rg.GetResource(handle);
	}

	RGResourceView RenderGraphResources::CreateShaderResourceView(RGResourceHandle handle, D3D12_SHADER_RESOURCE_VIEW_DESC const& desc)
	{
		return rg.CreateShaderResourceView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateRenderTargetView(RGResourceHandle handle, D3D12_RENDER_TARGET_VIEW_DESC const& desc)
	{
		return rg.CreateRenderTargetView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateUnorderedAccessView(RGResourceHandle handle, D3D12_UNORDERED_ACCESS_VIEW_DESC const& desc)
	{
		return rg.CreateUnorderedAccessView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateDepthStencilView(RGResourceHandle handle, D3D12_DEPTH_STENCIL_VIEW_DESC const& desc)
	{
		return rg.CreateDepthStencilView(handle, desc);
	}

}

