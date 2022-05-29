#include "RenderGraphResources.h"
#include "RenderGraph.h"

namespace adria
{

	RenderGraphResources::RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass) : rg(rg), rg_pass(rg_pass)
	{}

	Texture& RenderGraphResources::GetTexture(RGTextureRef handle)
	{
		return *rg.GetTexture(handle);
	}

	Buffer& RenderGraphResources::GetBuffer(RGBufferRef handle)
	{
		return *rg.GetBuffer(handle);
	}


	RGBlackboard& RenderGraphResources::GetBlackboard()
	{
		return rg.GetBlackboard();
	}

	ResourceView RenderGraphResources::GetSRV(RGTextureRefSRV handle) const
	{
		return rg.GetSRV(handle);
	}

	ResourceView RenderGraphResources::GetUAV(RGTextureRefUAV handle) const
	{
		return rg.GetUAV(handle);
	}

	ResourceView RenderGraphResources::GetRTV(RGTextureRefRTV handle) const
	{
		return rg.GetRTV(handle);
	}

	ResourceView RenderGraphResources::GetDSV(RGTextureRefDSV handle) const
	{
		return rg.GetDSV(handle);
	}

}

