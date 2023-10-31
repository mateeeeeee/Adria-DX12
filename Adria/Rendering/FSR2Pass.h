#pragma once
#include "FSR2/ffx_fsr2.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FSR2Pass
	{
	public:
		explicit FSR2Pass(GfxDevice* gfx);
		~FSR2Pass();

		void AddPass(RenderGraph& rg, RGResourceName input);
		float GetUpscaleRatio() const;
		void OnResize(uint32 w, uint32 h)
		{
			if (width != w || height != h)
			{
				width = w, height = h;
				RecreateRenderDimensions();
				recreate_context = true;
			}
		}

		Vector2u GetRenderDimensions()  const { return Vector2u(render_width, render_height); }
		Vector2u GetDisplayDimensions() const { return Vector2u(width, height); }

	private:
		GfxDevice* gfx = nullptr;
		uint32 width, height;
		uint32 render_width, render_height;

		FfxFsr2ContextDescription context_desc = {};
		FfxFsr2Context context = {};
		bool recreate_context = true;

		FfxFsr2QualityMode quality_mode = FFX_FSR2_QUALITY_MODE_QUALITY;
		float custom_upscale_ratio = 1.0f;
		float sharpness = 0.5f;

	private:
		void CreateContext();
		void DestroyContext();
		void RecreateRenderDimensions();
	};
}