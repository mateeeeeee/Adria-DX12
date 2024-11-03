#pragma once
#include "FidelityFX/host/ffx_cacao.h"
#include "RenderGraph/RenderGraphResourceName.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FFXCACAOPass
	{
	public:
		FFXCACAOPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~FFXCACAOPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);

	private:
		Char name_version[16] = {};
		GfxDevice* gfx;
		Uint32 width, height;
		FfxInterface*			   ffx_interface;
		Sint32					   preset_id = 0;
		Bool                       use_downsampled_ssao = false;
		Bool                       generate_normals = false;
		FfxCacaoSettings		   cacao_settings{};
		FfxCacaoContextDescription cacao_context_desc{};
		FfxCacaoContext            cacao_context{};
		FfxCacaoContext            cacao_downsampled_context{};
		Bool                       context_created = false;

	private:
		void CreateContext();
		void DestroyContext();
	};
}