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

		bool SupportsShaderModel(GfxShaderModel sm) const
		{
			return shader_model >= sm;
		}

	private:
		
		RayTracingSupport ray_tracing_support = RayTracingSupport::TierNotSupported;
		VSRSupport vsr_support = VSRSupport::TierNotSupported;
		MeshShaderSupport mesh_shader_support = MeshShaderSupport::TierNotSupported;
		GfxShaderModel shader_model = SM_Unknown;
	};
}