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


	RGBlackboard& RenderGraphResources::GetBlackboard()
	{
		return rg.GetBlackboard();
	}

	ResourceView RenderGraphResources::GetSRV(RGTextureHandleSRV handle) const
	{
		return rg.GetSRV(handle);
	}

	ResourceView RenderGraphResources::GetUAV(RGTextureHandleUAV handle) const
	{
		return rg.GetUAV(handle);
	}

	ResourceView RenderGraphResources::GetRTV(RGTextureHandleRTV handle) const
	{
		return rg.GetRTV(handle);
	}

	ResourceView RenderGraphResources::GetDSV(RGTextureHandleDSV handle) const
	{
		return rg.GetDSV(handle);
	}

}

