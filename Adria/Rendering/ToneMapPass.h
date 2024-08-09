#pragma once
#include "TextureHandle.h"
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class ToneMapPass : public PostEffect
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
			float tonemap_exposure = 1.5f;
		};
	public:
		ToneMapPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const { return true; }
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		TonemapParams params;
		TextureHandle lens_dirt_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle tony_mc_mapface_lut_handle = INVALID_TEXTURE_HANDLE;
		std::unique_ptr<GfxComputePipelineState> tonemap_pso;

	private:
		void CreatePSO();
	};
}