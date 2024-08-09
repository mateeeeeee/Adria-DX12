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
		FFXCACAOPass(GfxDevice* gfx, uint32 w, uint32 h);
		~FFXCACAOPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(uint32 w, uint32 h);

	private:
		char name_version[16] = {};
		GfxDevice* gfx;
		uint32 width, height;
		FfxInterface*			   ffx_interface;
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