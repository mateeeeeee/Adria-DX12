#pragma once
#include "FidelityFX/host/ffx_cas.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class CASPass
	{
	public:
		CASPass(GfxDevice* gfx, uint32 w, uint32 h);
		~CASPass();

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		char name_version[16] = {};
		GfxDevice* gfx;
		uint32 width, height;

		FfxCasContextDescription cas_context_desc{};
		FfxCasContext            cas_context{};
		float sharpness = 0.5f;

	private:
		void CreateContext();
		void DestroyContext();
	};
}