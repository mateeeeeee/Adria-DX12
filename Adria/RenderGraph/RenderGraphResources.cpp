#include "RenderGraphResources.h"
#include "RenderGraph.h"

namespace adria
{

	RenderGraphResources::RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass) : rg(rg), rg_pass(rg_pass)
	{}

	RGBlackboard& RenderGraphResources::GetBlackboard()
	{
		return rg.GetBlackboard();
	}

	Texture& RenderGraphResources::GetTexture(RGTextureRef handle)
	{
		return *rg.GetTexture(handle);
	}

	Buffer& RenderGraphResources::GetBuffer(RGBufferRef handle)
	{
		return *rg.GetBuffer(handle);
	}

	ResourceView RenderGraphResources::GetSRV(RGTextureSRVRef handle) const
	{
		return rg.GetSRV(handle);
	}

	ResourceView RenderGraphResources::GetUAV(RGTextureUAVRef handle) const
	{
		return rg.GetUAV(handle);
	}

	ResourceView RenderGraphResources::GetRTV(RGTextureRTVRef handle) const
	{
		return rg.GetRTV(handle);
	}

	ResourceView RenderGraphResources::GetDSV(RGTextureDSVRef handle) const
	{
		return rg.GetDSV(handle);
	}

}

