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
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;


	private:
		char name_version[20] = {};
		GfxDevice* gfx;
		Uint32 width, height;
		FfxInterface* ffx_interface;
		FfxDofContextDescription dof_context_desc{};
		FfxDofContext            dof_context{};

		float aperture = 0.01f;
		float focus_dist = 400.0f;
		float sensor_size = 0.02f;
		float coc_limit = 0.01f; 
		Sint32 quality = 10;
		bool  enable_ring_merge = false;

	private:

		void CreateContext();
		void DestroyContext();
	};

}