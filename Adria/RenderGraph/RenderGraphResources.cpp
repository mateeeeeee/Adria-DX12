#include "RenderGraphResources.h"
#include "RenderGraph.h"

namespace adria
{

	RenderGraphResources::RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass) : rg(rg), rg_pass(rg_pass)
	{}

	RGTexture& RenderGraphResources::GetTexture(RGResourceHandle handle)
	{
		return *rg.GetTexture(handle);
	}

	RGResourceView RenderGraphResources::CreateShaderResourceView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateShaderResourceView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateRenderTargetView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateRenderTargetView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateUnorderedAccessView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateUnorderedAccessView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateDepthStencilView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateDepthStencilView(handle, desc);
	}

}

