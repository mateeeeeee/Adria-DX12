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
	class Buffer;

	class DecalsPass
	{
	public:
		DecalsPass(tecs::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);

		void OnResize(uint32 w, uint32 h);

		void OnSceneInitialized(GraphicsDevice* gfx);

	private:
		tecs::registry& reg;
		TextureManager& texture_manager;
		uint32 width, height;
		std::unique_ptr<Buffer>	cube_vb = nullptr;
		std::unique_ptr<Buffer>	cube_ib = nullptr;

	private:
		void CreateCubeBuffers(GraphicsDevice* gfx);
	};
}