#pragma once
#include "GfxShaderEnums.h"

namespace adria
{
	class GfxDevice;

	enum class RayTracingSupport : uint8
	{
		TierNotSupported,
		Tier1_0,
		Tier1_1
	};
	enum class VSRSupport : uint8
	{
		TierNotSupported,
		Tier1,
		Tier2
	};
	enum class MeshShaderSupport : uint8
	{
		TierNotSupported,
		Tier1
	};

	enum class WorkGraphSupport : uint8
	{
		TierNotSupported,
		Tier1_0
	};

	class GfxCapabilities
	{
	public:
		bool Initialize(GfxDevice* gfx);

		bool SupportsRayTracing() const
		{
			return CheckRayTracingSupport(RayTracingSupport::Tier1_0);
		}
		bool SupportsMeshShaders() const
		{
			return CheckMeshShaderSupport(MeshShaderSupport::Tier1);
		}
		bool SupportsVSR() const
		{
			return CheckVSRSupport(VSRSupport::Tier1);
		}
		bool SupportsWorkGraphs() const
		{
			return CheckWorkGraphSupport(WorkGraphSupport::Tier1_0);
		}

		bool CheckRayTracingSupport(RayTracingSupport rts) const
		{
			return ray_tracing_support >= rts;
		}
		bool CheckVSRSupport(VSRSupport vsrs) const
		{
			return vsr_support >= vsrs;
		}
		bool CheckMeshShaderSupport(MeshShaderSupport mss) const
		{
			return mesh_shader_support >= mss;
		}
		bool CheckWorkGraphSupport(WorkGraphSupport wgs) const
		{
			return work_graph_support >= wgs;
		}

		bool SupportsShaderModel(GfxShaderModel sm) const
		{
			return shader_model >= sm;
		}
		bool SupportsEnhancedBarriers() const 
		{
			return enhanced_barriers_supported;
		}

		bool SupportsAdditionalShadingRates() const { return additional_shading_rates_supported; }
		uint32 GetShadingRateImageTileSize() const { return shading_rate_image_tile_size; }

	private:
		
		RayTracingSupport ray_tracing_support = RayTracingSupport::TierNotSupported;
		VSRSupport vsr_support = VSRSupport::TierNotSupported;
		MeshShaderSupport mesh_shader_support = MeshShaderSupport::TierNotSupported;
		WorkGraphSupport work_graph_support = WorkGraphSupport::TierNotSupported;
		GfxShaderModel shader_model = SM_Unknown;
		bool enhanced_barriers_supported = false;

		bool additional_shading_rates_supported = false;
		uint32 shading_rate_image_tile_size = 0;
	};
}