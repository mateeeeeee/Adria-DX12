#pragma once
#include "PostEffect.h"
#include "FidelityFX/host/ffx_vrs.h"
#include "Graphics/GfxShadingRate.h"
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceName.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;
	class GfxTexture;
	class GfxGraphicsPipelineState;
	class PostProcessor;

	class FFXVRSPass : public PostEffect
	{
	public:
		FFXVRSPass(GfxDevice* gfx, uint32 w, uint32 h);
		~FFXVRSPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void GUI() override;
		virtual void OnResize(uint32 w, uint32 h) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual bool IsSupported() const override { return is_supported; }

	private:
		char name_version[16] = {};
		GfxDevice* gfx;
		uint32 width, height;

		FfxInterface*			 ffx_interface;
		FfxVrsContextDescription vrs_context_description;
		FfxVrsContext            vrs_context;
		bool                     context_created = false;

		std::unique_ptr<GfxTexture> vrs_image;
		GfxDescriptor vrs_image_srv;
		std::unique_ptr<GfxGraphicsPipelineState> vrs_overlay_pso;

		bool is_supported = false;
		bool  additional_shading_rates_supported = false;
		uint32 shading_rate_image_tile_size;

	private:
		void CreateVRSImage();
		void CreateOverlayPSO();
		void DestroyContext();
		void CreateContext();
	};
}