#include "IDenoiserPass.h"
#include "OIDNDenoiserPass.h"

namespace adria
{

	IDenoiserPass* CreateDenoiser(GfxDevice* gfx, DenoiserType type)
	{
		switch (type)
		{
		case DenoiserType_None: return new DummyDenoiserPass();
		case DenoiserType_OIDN: return new OIDNDenoiserPass(gfx);
		}
		ADRIA_ASSERT(false);
		return nullptr;
	}

}
