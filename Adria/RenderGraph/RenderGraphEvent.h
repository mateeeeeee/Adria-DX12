#pragma once


namespace adria
{
	struct RenderGraphEvent
	{
		Char const* name;
	};
	using RGEvent = RenderGraphEvent;

	class RenderGraph;
	class RenderGraphScope
	{
	public:
		RenderGraphScope(RenderGraph& rg, Char const* name);
		~RenderGraphScope();
	private:
		RenderGraph& rg;
	};
	using RGScope = RenderGraphScope;
	#define RG_SCOPE(graph, name) RGScope ADRIA_CONCAT(_rgscope_,__COUNTER__)(graph, name)
}