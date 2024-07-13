#pragma once
#include <memory>
#include "SkyModel.h"
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxBuffer;
	class GfxTexture;
	class GraphicsPipelineState;
	class ComputePipelineState;

	enum class SkyType : uint8
	{
		Skybox,
		MinimalAtmosphere,
		HosekWilkie
	};

	class SkyPass
	{
	public:
		SkyPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h);

		void AddComputeSkyPass(RenderGraph& rg, Vector3 const& dir);
		void AddDrawSkyPass(RenderGraph& rg);

		void OnSceneInitialized();
		void OnResize(uint32 w, uint32 h);

		int32 GetSkyIndex() const;
		SkyType GetSkyType() const { return sky_type; }

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxBuffer>	cube_vb = nullptr;
		std::unique_ptr<GfxBuffer>	cube_ib = nullptr;

		std::unique_ptr<GfxTexture> sky_texture = nullptr;
		GfxDescriptor sky_texture_srv;

		std::unique_ptr<ComputePipelineState> minimal_atmosphere_pso;
		std::unique_ptr<ComputePipelineState> hosek_wilkie_pso;
		std::unique_ptr<GraphicsPipelineState> sky_pso;
		
		SkyType sky_type;
		float turbidity = 2.0f;
		float ground_albedo = 0.1f;

	private:
		void CreatePSOs();
		void CreateCubeBuffers();
	};
}