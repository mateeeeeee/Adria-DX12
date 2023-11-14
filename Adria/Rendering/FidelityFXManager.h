#pragma once
#include <memory>
#include "FidelityFX/host/ffx_interface.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FFXDepthOfFieldPass;
	class FFXCASPass;
	class FSR2Pass;

	class FidelityFXManager
	{
	public:
		FidelityFXManager(GfxDevice* gfx, uint32 width, uint32 height);
		~FidelityFXManager();

		FSR2Pass& GetFSR2() { return *ffx_fsr2; }
		FFXDepthOfFieldPass& GetDoF() { return *ffx_depth_of_field; }
		FFXCASPass& GetCAS() { return *ffx_cas; }

	private:
		FfxInterface ffx_interface;
		std::unique_ptr<FFXDepthOfFieldPass> ffx_depth_of_field;
		std::unique_ptr<FFXCASPass>			 ffx_cas;
		std::unique_ptr<FSR2Pass>			 ffx_fsr2;
	};
}