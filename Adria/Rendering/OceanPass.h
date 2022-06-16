#pragma once
#include <memory>
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class TextureManager;
	class GraphicsDevice;
	class Texture;

	class OceanPass
	{
		static constexpr uint32 FFT_RESOLUTION = 512;

	public:
		OceanPass(tecs::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h);

		void UpdateOceanColor(float32(&color)[3]);

		void AddPasses(RenderGraph& rendergraph, bool recreate_spectrum, bool tesselated, bool wireframe);

		void OnResize(uint32 w, uint32 h);

		void OnSceneInitialized(GraphicsDevice* gfx);

	private:
		tecs::registry& reg;
		TextureManager& texture_manager;
		size_t foam_handle = -1;
		size_t perlin_handle = -1;
		uint32 width, height;
		std::unique_ptr<Texture> initial_spectrum;
		std::unique_ptr<Texture> ping_pong_phase_textures[2];
		bool pong_phase = false;
		std::unique_ptr<Texture> ping_pong_spectrum_textures[2];
		bool pong_spectrum = false;
	};
}