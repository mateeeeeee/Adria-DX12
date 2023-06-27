#pragma once

#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;

	class ToneMapPass
	{
		enum class ToneMap : uint8
		{
			Reinhard,
			Hable,
			Linear
		};

		struct TonemapParams
		{
			ToneMap tone_map_op = ToneMap::Reinhard;
			float tonemap_exposure = 1.0f;
		};
	public:
		ToneMapPass(uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src, RGResourceName output);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized(GfxDevice* gfx);
	private:
		uint32 width, height;
		TonemapParams params;
		size_t lens_dirt_handle = -1;
	private:
		void GUI();
	};
}