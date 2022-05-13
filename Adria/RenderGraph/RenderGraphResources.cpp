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

}

