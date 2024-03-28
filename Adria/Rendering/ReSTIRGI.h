#pragma once
#include "Graphics/GfxDescriptor.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class GfxBuffer;
	class GfxTexture;
	class GfxDevice;
	class RenderGraph;

	class ReSTIRGI
	{
	public:
		ReSTIRGI(GfxDevice* gfx, uint32 width, uint32 height) : gfx(gfx), width(width), height(height)
		{

		}

		void AddPasses(RenderGraph& rg)
		{

		}

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

		int32 GetIrradianceIndex() const { return -1; }

	private:
		GfxDevice* gfx;
		uint32 width, height;

	};
}