#pragma once
#include <memory>
#include "SkyModel.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxBuffer;
	enum class SkyType : uint8;


	class SkyPass
	{
	public:
		SkyPass(entt::registry& reg, uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, DirectX::XMFLOAT3 const& dir);

		void OnSceneInitialized(GfxDevice* gfx);
		void OnResize(uint32 w, uint32 h);

		SkyType GetSkyType() const { return sky_type; }
	private:
		entt::registry& reg;
		uint32 width, height;
		std::unique_ptr<GfxBuffer>	cube_vb = nullptr;
		std::unique_ptr<GfxBuffer>	cube_ib = nullptr;

		SkyType sky_type;
		float turbidity = 2.0f;
		float ground_albedo = 0.1f;
	private:
		void CreateCubeBuffers(GfxDevice* gfx);
	};
}