#pragma once
#include "TextureHandle.h"
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
			Linear,
			TonyMcMapface
		};

		struct TonemapParams
		{
			ToneMap tone_map_op = ToneMap::TonyMcMapface;
			float tonemap_exposure = 2.5f;
		};
	public:
		ToneMapPass(uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src);
		void AddPass(RenderGraph& rg, RGResourceName hdr_src, RGResourceName output);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
	private:
		uint32 width, height;
		TonemapParams params;
		TextureHandle lens_dirt_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle tony_mc_mapface_lut_handle = INVALID_TEXTURE_HANDLE;

	private:
		void GUI();
	};
}