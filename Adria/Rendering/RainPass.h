#pragma once


namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class RainPass
	{
	public:
		RainPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
		{
		
		}

		void AddPass(RenderGraph& rg);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		GfxDevice* gfx;
		uint32 width;
		uint32 height;
	};
}