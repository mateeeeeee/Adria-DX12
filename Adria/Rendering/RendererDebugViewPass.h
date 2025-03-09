#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "Utilities/Delegate.h"

namespace adria
{
	enum class RendererDebugView : Uint32
	{
		Final,
		Diffuse,
		WorldNormal,
		Depth,
		Roughness,
		Metallic,
		Emissive,
		MaterialID,
		MeshletID,
		AmbientOcclusion,
		IndirectLighting,
		Custom,
		ShadingExtension,
		ViewMipMaps,
		TriangleOverdraw,
		MotionVectors,
		Count
	};

	DECLARE_EVENT(DebugViewChangedEvent, RendererDebugViewPass, RendererDebugView)

	class GfxDevice;
	class RenderGraph;

	class RendererDebugViewPass
	{
	public:
		RendererDebugViewPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~RendererDebugViewPass();

		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
		}
		void AddPass(RenderGraph&);
		void GUI();

		void SetDebugView(RendererDebugView value);
		RendererDebugView GetDebugView() const { return debug_view; }
		DebugViewChangedEvent& GetDebugViewChangedEvent() { return debug_view_changed_event; }

	private:
		GfxDevice* gfx;
		Uint32 width;
		Uint32 height;
		std::unique_ptr<GfxComputePipelineStatePermutations> renderer_output_psos;
		Float triangle_overdraw_scale = 1.0f;
		RendererDebugView debug_view = RendererDebugView::Final;
		DebugViewChangedEvent debug_view_changed_event;

	private:
		void CreatePSOs();
	};
}