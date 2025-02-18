#include "RenderGraphEvent.h"
#include "RenderGraph.h"

namespace adria
{
	RenderGraphScope::RenderGraphScope(RenderGraph& rg, Char const* name) : rg(rg)
	{
		rg.PushEvent(name);
	}
	RenderGraphScope::~RenderGraphScope()
	{
		rg.PopEvent();
	}

}
