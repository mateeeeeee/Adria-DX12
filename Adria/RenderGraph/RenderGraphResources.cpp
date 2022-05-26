#include "RenderGraphResources.h"
#include "RenderGraph.h"

namespace adria
{

	RenderGraphResources::RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass) : rg(rg), rg_pass(rg_pass)
	{}

	Texture& RenderGraphResources::GetTexture(RGTextureHandle handle)
	{
		return *rg.GetTexture(handle);
	}

	Buffer& RenderGraphResources::GetBuffer(RGBufferHandle handle)
	{
		return *rg.GetBuffer(handle);
	}

	RGResourceView RenderGraphResources::CreateShaderResourceView(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateShaderResourceView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateRenderTargetView(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateRenderTargetView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateUnorderedAccessView(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateUnorderedAccessView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateDepthStencilView(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateDepthStencilView(handle, desc);
	}


	RGResourceView RenderGraphResources::CreateShaderResourceView(RGBufferHandle handle, BufferViewDesc const& desc)
	{
		return rg.CreateShaderResourceView(handle, desc);
	}

	RGResourceView RenderGraphResources::CreateUnorderedAccessView(RGBufferHandle handle, BufferViewDesc const& desc)
	{
		return rg.CreateUnorderedAccessView(handle, desc);
	}



}

