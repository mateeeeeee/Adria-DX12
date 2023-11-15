#pragma once
#include "FidelityFX/host/ffx_cacao.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FFXCACAOPass
	{
	public:
		FFXCACAOPass(GfxDevice* gfx, FfxInterface& ffx_interface, uint32 w, uint32 h);
		~FFXCACAOPass();

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		char name_version[16] = {};
		GfxDevice* gfx;
		uint32 width, height;

		int32					   preset_id = 0;
		bool                       use_downsampled_ssao = false;
		bool                       generate_normals = false;
		FfxCacaoSettings		   cacao_settings{};
		FfxCacaoContextDescription cacao_context_desc{};
		FfxCacaoContext            cacao_context{};
		FfxCacaoContext            cacao_downsampled_context{};
		bool                       context_created = false;

	private:
		void CreateContext();
		void DestroyContext();
	};
}