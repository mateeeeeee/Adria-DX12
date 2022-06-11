#pragma once
#include "Enums.h"
#include "RendererSettings.h"
#include "BlurPass.h"
#include "CopyToTexturePass.h"
#include "AddTexturesPass.h"
#include "GenerateMipsPass.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../tecs/entity.h"


namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class GPUProfiler;
	class TextureManager;
	struct Light;

	class Postprocessor
	{

	public:
		Postprocessor(tecs::registry& reg, TextureManager& texture_manager, uint32 width, uint32 height);

		void AddPasses(RenderGraph& rg, PostprocessSettings const& settings);

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

		RGResourceName GetFinalResource() const;
	private:
		tecs::registry& reg;
		TextureManager& texture_manager;
		uint32 width, height;
		std::vector<size_t> cloud_textures;
		RGResourceName final_resource;

		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		GenerateMipsPass generate_mips_pass;
	private:
		void AddCopyHDRPass(RenderGraph& rg);
		void AddVolumetricCloudsPass(RenderGraph& rg);
		void AddSSRPass(RenderGraph& rg);
		void AddFogPass(RenderGraph& rg);
		void AddBloomPass(RenderGraph& rg);
		void AddSunPass(RenderGraph& rg, tecs::entity sun);
		void AddGodRaysPass(RenderGraph& rg, Light const& light);
		void AddDepthOfFieldPass(RenderGraph& rg);	
		void AddGenerateBokehPasses(RenderGraph& rg);
		void AddDrawBokehPass(RenderGraph& rg);
	};
}