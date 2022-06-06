#pragma once
#include "Enums.h"
#include "RendererSettings.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceRef.h"


namespace adria
{
	class RenderGraph;
	class GPUProfiler;

	struct PostprocessData
	{
		RGTextureRef final_texture;
	};

	class Postprocessor
	{
		struct CopyHDRPassData
		{
			RGTextureRef src_texture;
			RGTextureRef dst_texture;
		};

	public:
		Postprocessor(uint32 width, uint32 height)
			: width(width), height(height)
		{}

		PostprocessData const& AddPasses(RenderGraph& rg, PostprocessSettings const& settings,
			RGTextureRef hdr_texture)
		{
			PostprocessData data{};
			CopyHDRPassData const& copy_data = AddCopyHDRPass(rg, hdr_texture);
			data.final_texture = copy_data.dst_texture;

			return data;
		}

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		uint32 width, height;

	private:

		CopyHDRPassData const& AddCopyHDRPass(RenderGraph& rg, RGTextureRef hdr_texture);
	};
}