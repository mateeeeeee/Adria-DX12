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
			Int32 shape_noise_frequency = 4;
			Int32 shape_noise_resolution = 128;
			Int32 detail_noise_frequency = 6;
			Int32 detail_noise_resolution = 32;

			Int32 max_num_steps = 64;
			Float cloud_min_height = 1500.0f;
			Float cloud_max_height = 4000.0f;
			Float shape_noise_scale = 0.3f;
			Float detail_noise_scale = 3.2f;
			Float detail_noise_modifier = 0.33f;
			Float cloud_coverage = 0.625f;
			Float cloud_type = 0.5f;
			Float global_density = 0.25f;

			Float planet_radius = 35000.0f;
			Float light_step_length = 64.0f;
			Float light_cone_radius = 0.4f;

			Float cloud_base_color[3] = { 0.78f, 0.86f, 1.0f };
			Float cloud_top_color[3] = { 1.0f, 1.0f, 1.0f };
			Float precipitation = 1.78f;
			Float ambient_light_factor = 0.12f;
			Float sun_light_factor = 0.7f;
			Float henyey_greenstein_g_forward = 0.4f;
			Float henyey_greenstein_g_backward = 0.179f;
		};

		enum CloudResolution
		{
			CloudResolution_Full  = 0,
			CloudResolution_Half = 1,
			CloudResolution_Quarter = 2
		};

	public:
		VolumetricCloudsPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~VolumetricCloudsPass();

		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

		void OnRainEvent(Bool enabled);

	private:
		GfxDevice* gfx;
		Uint32 width, height;

		std::unique_ptr<GfxTexture> prev_clouds;
		std::unique_ptr<GfxTexture> cloud_detail_noise;
		std::unique_ptr<GfxTexture> cloud_shape_noise;
		std::unique_ptr<GfxTexture> cloud_type;

		CloudParameters params{};
		CloudResolution resolution = CloudResolution_Full;
		Bool should_generate_textures = false;
		Bool temporal_reprojection = true;
		std::unique_ptr<GfxComputePipelineStatePermutations> clouds_psos;
		std::unique_ptr<GfxComputePipelineState> clouds_type_pso;
		std::unique_ptr<GfxComputePipelineState> clouds_shape_pso;
		std::unique_ptr<GfxComputePipelineState> clouds_detail_pso;
		std::unique_ptr<GfxGraphicsPipelineState> clouds_combine_pso;

	private:
		void CreatePSOs();
		void CreateCloudTextures(GfxDevice* gfx = nullptr);

		void AddComputeTexturesPass(RenderGraph&);
		void AddComputeCloudsPass(RenderGraph&);
		void AddCopyPassForTemporalReprojection(RenderGraph&);
		void AddCombinePass(RenderGraph& rg, RGResourceName render_target);
	};

}