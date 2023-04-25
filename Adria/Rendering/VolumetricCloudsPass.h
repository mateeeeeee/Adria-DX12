#pragma once
#include <vector>
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;

	class VolumetricCloudsPass
	{
		struct CloudParameters
		{
			float crispiness = 43.0f;
			float curliness = 3.6f;
			float coverage = 0.25f;
			float light_absorption = 0.003f;
			float clouds_bottom_height = 4000.0f;
			float clouds_top_height = 10000.0f;
			float density_factor = 0.015f;
			float cloud_type = 0.9f;
		};
	public:
		VolumetricCloudsPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void AddCombinePass(RenderGraph& rendergraph, RGResourceName render_target);
		void OnResize(GfxDevice* gfx, uint32 w, uint32 h);
		void OnSceneInitialized(GfxDevice* gfx);

	private:
		uint32 width, height;
		std::vector<size_t> cloud_textures;
		std::unique_ptr<GfxTexture> prev_clouds;
		CloudParameters params{};
		bool temporal_reprojection = false;

	};

}