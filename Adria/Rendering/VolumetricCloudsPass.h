#pragma once
#include "PostEffect.h"
#include "Graphics/GfxPipelineStatePermutationsFwd.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;

	class VolumetricCloudsPass : public PostEffect
	{
		struct CloudParameters
		{
			int32 shape_noise_frequency = 4;
			int32 shape_noise_resolution = 128;
			int32 detail_noise_frequency = 6;
			int32 detail_noise_resolution = 32;

			int32 max_num_steps = 64;
			float cloud_min_height = 1500.0f;
			float cloud_max_height = 4000.0f;
			float shape_noise_scale = 0.3f;
			float detail_noise_scale = 3.2f;
			float detail_noise_modifier = 0.33f;
			float cloud_coverage = 0.625f;
			float cloud_type = 0.5f;
			float global_density = 0.25f;

			float planet_radius = 35000.0f;
			float light_step_length = 64.0f;
			float light_cone_radius = 0.4f;

			float cloud_base_color[3] = { 0.78f, 0.86f, 1.0f };
			float cloud_top_color[3] = { 1.0f, 1.0f, 1.0f };
			float precipitation = 1.78f;
			float ambient_light_factor = 0.12f;
			float sun_light_factor = 0.7f;
			float henyey_greenstein_g_forward = 0.4f;
			float henyey_greenstein_g_backward = 0.179f;
		};

		enum CloudResolution
		{
			CloudResolution_Full  = 0,
			CloudResolution_Half = 1,
			CloudResolution_Quarter = 2
		};

	public:
		VolumetricCloudsPass(GfxDevice* gfx, uint32 w, uint32 h);
		~VolumetricCloudsPass();

		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32 w, uint32 h) override;
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

		void OnRainEvent(bool enabled);

	private:
		GfxDevice* gfx;
		uint32 width, height;

		std::unique_ptr<GfxTexture> prev_clouds;
		std::unique_ptr<GfxTexture> cloud_detail_noise;
		std::unique_ptr<GfxTexture> cloud_shape_noise;
		std::unique_ptr<GfxTexture> cloud_type;

		CloudParameters params{};
		CloudResolution resolution = CloudResolution_Full;
		bool should_generate_textures = false;
		bool temporal_reprojection = true;
		std::unique_ptr<GfxComputePipelineStatePermutations> clouds_psos;
		std::unique_ptr<GfxComputePipelineState> clouds_type_pso;
		std::unique_ptr<GfxComputePipelineState> clouds_shape_pso;
		std::unique_ptr<GfxComputePipelineState> clouds_detail_pso;
		std::unique_ptr<GfxGraphicsPipelineState> clouds_combine_pso;

	private:
		void CreatePSOs();
		void CreateCloudTextures(GfxDevice* gfx = nullptr);
		void AddCombinePass(RenderGraph& rendergraph, RGResourceName render_target);

	};

}