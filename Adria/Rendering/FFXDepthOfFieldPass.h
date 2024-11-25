#pragma once
#include "FidelityFX/host/ffx_dof.h" 
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FFXDepthOfFieldPass : public PostEffect
	{
		
	public:
		FFXDepthOfFieldPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~FFXDepthOfFieldPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;


	private:
		Char name_version[20] = {};
		GfxDevice* gfx;
		Uint32 width, height;
		FfxInterface* ffx_interface;
		FfxDofContextDescription dof_context_desc{};
		FfxDofContext            dof_context{};

		Float aperture = 0.01f;
		Float focus_dist = 400.0f;
		Float sensor_size = 0.02f;
		Float coc_limit = 0.01f; 
		Int32 quality = 10;
		Bool  enable_ring_merge = false;

	private:

		void CreateContext();
		void DestroyContext();
	};

}