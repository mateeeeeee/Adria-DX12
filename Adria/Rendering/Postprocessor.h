#pragma once
#include <memory>
#include <d3d12.h>
#include <wrl.h>
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
	class Buffer;
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
		size_t hex_bokeh_handle;
		size_t oct_bokeh_handle;
		size_t circle_bokeh_handle;
		size_t cross_bokeh_handle;

		RGResourceName final_resource;
		std::unique_ptr<Texture> history_buffer;
		std::unique_ptr<Buffer> counter_reset_buffer;
		std::unique_ptr<Buffer> bokeh_indirect_buffer;
		Microsoft::WRL::ComPtr<ID3D12CommandSignature> bokeh_command_signature;
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

		void AddGenerateBokehPass(RenderGraph& rg);
		void AddDrawBokehPass(RenderGraph& rg);
	};
}