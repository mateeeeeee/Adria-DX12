#include "IDenoiserPass.h"
#include "OIDNDenoiserPass.h"

namespace adria
{

	IDenoiserPass* CreateDenoiser(GfxDevice* gfx, DenoiserType type)
	{
		IDenoiserPass* denoiser = nullptr;
		switch (type)
		{
		case DenoiserType_None: denoiser = new DummyDenoiserPass();   break;
		case DenoiserType_OIDN: denoiser = new OIDNDenoiserPass(gfx); break;
		}
		if (!denoiser->IsSupported())
		{
			delete denoiser;
			denoiser = new DummyDenoiserPass();
		}
		return denoiser;
	}
}
