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
		FFXDepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h);
		~FFXDepthOfFieldPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;


	private:
		char name_version[20] = {};
		GfxDevice* gfx;
		uint32 width, height;
		FfxInterface* ffx_interface;
		FfxDofContextDescription dof_context_desc{};
		FfxDofContext            dof_context{};

		float aperture = 0.01f;
		float focus_dist = 400.0f;
		float sensor_size = 0.02f;
		float coc_limit = 0.01f; 
		int32 quality = 10;
		bool  enable_ring_merge = false;

	private:

		void CreateContext();
		void DestroyContext();
	};

}