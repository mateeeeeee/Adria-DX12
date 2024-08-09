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
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;

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

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
		void GUI();

		int32 GetSkyIndex() const;
		void SetSkyType(SkyType type)
		{
			sky_type = type;
		}

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxBuffer>	cube_vb = nullptr;
		std::unique_ptr<GfxBuffer>	cube_ib = nullptr;

		std::unique_ptr<GfxTexture> sky_texture = nullptr;
		GfxDescriptor sky_texture_srv;

		std::unique_ptr<GfxComputePipelineState> minimal_atmosphere_pso;
		std::unique_ptr<GfxComputePipelineState> hosek_wilkie_pso;
		std::unique_ptr<GfxGraphicsPipelineState> sky_pso;
		
		SkyType sky_type = SkyType::HosekWilkie;
		float turbidity = 2.0f;
		float ground_albedo = 0.1f;

	private:
		void CreatePSOs();
		void CreateCubeBuffers();
	};
}