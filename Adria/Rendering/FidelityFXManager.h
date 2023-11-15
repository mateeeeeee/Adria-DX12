#pragma once
#include <memory>

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FFXCACAOPass;
	class FFXDepthOfFieldPass;
	class FFXCASPass;
	class FSR2Pass;


	class FidelityFXManager
	{
	public:
		FidelityFXManager(GfxDevice* gfx, uint32 width, uint32 height);
		~FidelityFXManager();

		FFXCACAOPass& GetCACAO() { return *ffx_cacao; }
		FFXDepthOfFieldPass& GetDoF() { return *ffx_depth_of_field; }
		FFXCASPass& GetCAS() { return *ffx_cas; }
		FSR2Pass& GetFSR2() { return *ffx_fsr2; }

	private:
		std::unique_ptr<FfxInterface>		 ffx_interface;
		std::unique_ptr<FFXCACAOPass>		 ffx_cacao;
		std::unique_ptr<FFXDepthOfFieldPass> ffx_depth_of_field;
		std::unique_ptr<FFXCASPass>			 ffx_cas;
		std::unique_ptr<FSR2Pass>			 ffx_fsr2;
	};
}