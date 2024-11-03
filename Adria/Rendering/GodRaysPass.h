#pragma once
#include "PostEffect.h"
#include "SunPass.h"
#include "HelperPasses.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;
	struct Light;

	class GodRaysPass : public PostEffect
	{
	public:
		GodRaysPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual Bool IsEnabled(PostProcessor const*) const override
		{
			return true;
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> god_rays_pso;
		CopyToTexturePass copy_to_texture_pass;

	private:
		void CreatePSO();
		void AddGodRaysPass(RenderGraph&, Light const&);
	};


}