#pragma once
#include "GfxShaderEnums.h"

namespace adria
{
	class GfxDevice;

	enum class RayTracingSupport : Uint8
	{
		TierNotSupported,
		Tier1_0,
		Tier1_1
	};
	enum class VSRSupport : Uint8
	{
		TierNotSupported,
		Tier1,
		Tier2
	};
	enum class MeshShaderSupport : Uint8
	{
		TierNotSupported,
		Tier1
	};

	enum class WorkGraphSupport : Uint8
	{
		TierNotSupported,
		Tier1_0
	};

	class GfxCapabilities
	{
	public:
		Bool Initialize(GfxDevice* gfx);

		Bool SupportsRayTracing() const
		{
			return CheckRayTracingSupport(RayTracingSupport::Tier1_0);
		}
		Bool SupportsMeshShaders() const
		{
			return CheckMeshShaderSupport(MeshShaderSupport::Tier1);
		}
		Bool SupportsVSR() const
		{
			return CheckVSRSupport(VSRSupport::Tier1);
		}
		Bool SupportsWorkGraphs() const
		{
			return CheckWorkGraphSupport(WorkGraphSupport::Tier1_0);
		}

		Bool CheckRayTracingSupport(RayTracingSupport rts) const
		{
			return ray_tracing_support >= rts;
		}
		Bool CheckVSRSupport(VSRSupport vsrs) const
		{
			return vsr_support >= vsrs;
		}
		Bool CheckMeshShaderSupport(MeshShaderSupport mss) const
		{
			return mesh_shader_support >= mss;
		}
		Bool CheckWorkGraphSupport(WorkGraphSupport wgs) const
		{
			return work_graph_support >= wgs;
		}

		Bool SupportsShaderModel(GfxShaderModel sm) const
		{
			return shader_model >= sm;
		}
		Bool SupportsEnhancedBarriers() const 
		{
			return enhanced_barriers_supported;
		}

		Bool SupportsAdditionalShadingRates() const { return additional_shading_rates_supported; }
		Uint32 GetShadingRateImageTileSize() const { return shading_rate_image_tile_size; }

	private:
		
		RayTracingSupport ray_tracing_support = RayTracingSupport::TierNotSupported;
		VSRSupport vsr_support = VSRSupport::TierNotSupported;
		MeshShaderSupport mesh_shader_support = MeshShaderSupport::TierNotSupported;
		WorkGraphSupport work_graph_support = WorkGraphSupport::TierNotSupported;
		GfxShaderModel shader_model = SM_Unknown;
		Bool enhanced_barriers_supported = false;

		Bool additional_shading_rates_supported = false;
		Uint32 shading_rate_image_tile_size = 0;
	};
}