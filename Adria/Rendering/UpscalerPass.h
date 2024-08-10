#pragma once
#include "PostEffect.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class UpscalerPass : public PostEffect
	{
		DECLARE_EVENT(RenderResolutionChanged, UpscalerPass, uint32, uint32)
	public:
		RenderResolutionChanged& GetRenderResolutionChangedEvent() { return render_resolution_changed_event; }

	private:
		RenderResolutionChanged render_resolution_changed_event;

	protected:
		void BroadcastRenderResolutionChanged(uint32 w, uint32 h)
		{
			render_resolution_changed_event.Broadcast(w, h);
		}
	};

	class EmptyUpscalerPass : public UpscalerPass
	{
	public:
		EmptyUpscalerPass() {}
		virtual void AddPass(RenderGraph&, PostProcessor*) {}
		virtual void OnResize(uint32, uint32) {}
		virtual bool IsEnabled(PostProcessor const*) const { return false; }
	};
}