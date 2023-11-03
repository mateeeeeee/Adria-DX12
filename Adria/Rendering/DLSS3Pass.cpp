#include "DLSS3Pass.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Logging/Logger.h"

namespace adria
{

	DLSS3Pass::DLSS3Pass(GfxDevice* gfx, uint32 w, uint32 h) 
		: gfx(gfx), display_width(), display_height(), render_width(), render_height()
	{

	}

	DLSS3Pass::~DLSS3Pass()
	{

	}

	RGResourceName DLSS3Pass::AddPass(RenderGraph& rg, RGResourceName input)
	{

	}

}

