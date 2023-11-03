#pragma once
#include "RenderGraph/RenderGraphResourceName.h"
#include "Events/Delegate.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class DLSS3Pass
	{
		DECLARE_EVENT(RenderResolutionChanged, FSR2Pass, uint32, uint32);
	public:
		DLSS3Pass(GfxDevice* gfx, uint32 w, uint32 h);
		~DLSS3Pass();

		RGResourceName AddPass(RenderGraph& rg, RGResourceName input);

		void OnResize(uint32 w, uint32 h)
		{
			display_width = w, display_height = h;
			
		}

		Vector2u GetRenderResolution()  const { return Vector2u(render_width, render_height); }
		Vector2u GetDisplayResolution() const { return Vector2u(display_width, display_height); }

		RenderResolutionChanged& GetRenderResolutionChangedEvent() { return render_resolution_changed_event; }

		bool IsSupported() const { return is_supported; }

	private:
		bool is_supported = false;
		char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		uint32 display_width, display_height;
		uint32 render_width, render_height;

		

		RenderResolutionChanged render_resolution_changed_event;
	};
}