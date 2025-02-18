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

	enum class SkyType : Uint8
	{
		Skybox,
		MinimalAtmosphere,
		HosekWilkie
	};

	class SkyPass
	{
	public:
		SkyPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);

		void AddPasses(RenderGraph& rg, Vector3 const& dir);

		void OnResize(Uint32 w, Uint32 h);
		void OnSceneInitialized();
		void GUI();

		Int32 GetSkyIndex() const;
		void SetSkyType(SkyType type)
		{
			sky_type = type;
		}

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxBuffer>	cube_vb = nullptr;
		std::unique_ptr<GfxBuffer>	cube_ib = nullptr;

		std::unique_ptr<GfxTexture> sky_texture = nullptr;
		GfxDescriptor sky_texture_srv;

		std::unique_ptr<GfxComputePipelineState> minimal_atmosphere_pso;
		std::unique_ptr<GfxComputePipelineState> hosek_wilkie_pso;
		std::unique_ptr<GfxGraphicsPipelineState> sky_pso;
		
		SkyType sky_type = SkyType::HosekWilkie;
		Float turbidity = 2.0f;
		Float ground_albedo = 0.1f;

	private:
		void CreatePSOs();
		void CreateCubeBuffers();

		void AddComputeSkyPass(RenderGraph& rg, Vector3 const& dir);
		void AddDrawSkyPass(RenderGraph& rg);
	};
}