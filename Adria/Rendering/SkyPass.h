#pragma once
#include <memory>
#include "SkyModel.h"
#include "../Graphics/DescriptorHeap.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class RenderGraph;
	class TextureManager;
	class GraphicsDevice;
	class Buffer;
	enum class ESkyType : uint8;


	class SkyPass
	{
	public:
		SkyPass(entt::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, DirectX::XMFLOAT3 const& dir);

		void OnSceneInitialized(GraphicsDevice* gfx);
		void OnResize(uint32 w, uint32 h);

		ESkyType GetSkyType() const { return sky_type; }
	private:
		entt::registry& reg;
		TextureManager& texture_manager;
		uint32 width, height;
		std::unique_ptr<Buffer>	cube_vb = nullptr;
		std::unique_ptr<Buffer>	cube_ib = nullptr;

		ESkyType sky_type;
		float sky_color[3] = { 0.53f, 0.81f, 0.92f };
		float turbidity = 2.0f;
		float ground_albedo = 0.1f;
	private:
		void CreateCubeBuffers(GraphicsDevice* gfx);
	};
}