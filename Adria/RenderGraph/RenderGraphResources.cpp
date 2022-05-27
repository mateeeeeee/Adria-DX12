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


	ResourceView RenderGraphResources::GetSRV(RGTextureHandleSRV handle) const
	{

	}

	ResourceView RenderGraphResources::GetUAV(RGTextureHandleUAV handle) const
	{

	}

	ResourceView RenderGraphResources::GetRTV(RGTextureHandleRTV handle) const
	{

	}

	ResourceView RenderGraphResources::GetDSV(RGTextureHandleDSV handle) const
	{

	}

}

