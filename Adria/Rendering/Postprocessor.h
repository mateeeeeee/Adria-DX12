#pragma once
#include <memory>
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
	class GraphicsDevice;
	class GPUProfiler;
	class TextureManager;
	class Texture;
	struct Light;

	class Postprocessor
	{

	public:
		Postprocessor(tecs::registry& reg, TextureManager& texture_manager, uint32 width, uint32 height);

		void AddPasses(RenderGraph& rg, PostprocessSettings const& settings);

		void OnResize(GraphicsDevice* gfx, uint32 w, uint32 h);
		void OnSceneInitialized(GraphicsDevice* gfx);

		RGResourceName GetFinalResource() const;
	private:
		tecs::registry& reg;
		TextureManager& texture_manager;
		uint32 width, height;
		PostprocessSettings settings;

		std::vector<size_t> cloud_textures;
		std::vector<size_t> lens_flare_textures;
		RGResourceName final_resource;
		std::unique_ptr<Texture> history_buffer;
		//helper passes
		BlurPass blur_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		GenerateMipsPass generate_mips_pass;
	private:
		void AddCopyHDRPass(RenderGraph& rg);
		void AddVelocityBufferPass(RenderGraph& rg);
		void AddVolumetricCloudsPass(RenderGraph& rg);
		void AddSSRPass(RenderGraph& rg);
		void AddFogPass(RenderGraph& rg);
		void AddBloomPass(RenderGraph& rg);
		void AddSunPass(RenderGraph& rg, tecs::entity sun);
		void AddGodRaysPass(RenderGraph& rg, Light const& light);
		void AddLensFlarePass(RenderGraph& rg, Light const& light);
		void AddDepthOfFieldPass(RenderGraph& rg);	
		void AddMotionBlurPass(RenderGraph& rg);
		void AddHistoryCopyPass(RenderGraph& rg);
		void AddTAAPass(RenderGraph& rg);

		void AddGenerateBokehPasses(RenderGraph& rg);
		void AddDrawBokehPass(RenderGraph& rg);
	};
}