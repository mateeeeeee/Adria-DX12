#include "d3dx12_check_feature_support.h"
#include "GfxCapabilities.h"
#include "GfxDevice.h"
#include "Logging/Logger.h"

namespace adria
{
	namespace
	{
		constexpr RayTracingSupport ConvertRayTracingTier(D3D12_RAYTRACING_TIER tier)
		{
			switch (tier)
			{
			case D3D12_RAYTRACING_TIER_NOT_SUPPORTED:
				return RayTracingSupport::TierNotSupported;
			case D3D12_RAYTRACING_TIER_1_0:
				return RayTracingSupport::Tier1_0;
			case D3D12_RAYTRACING_TIER_1_1:
				return RayTracingSupport::Tier1_1;
			}
			return RayTracingSupport::TierNotSupported;
		}
		constexpr VSRSupport ConvertVSRTier(D3D12_VARIABLE_SHADING_RATE_TIER tier)
		{
			switch (tier)
			{
			case D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED:
				return VSRSupport::TierNotSupported;
			case D3D12_VARIABLE_SHADING_RATE_TIER_1:
				return VSRSupport::Tier1;
			case D3D12_VARIABLE_SHADING_RATE_TIER_2:
				return VSRSupport::Tier2;
			}
			return VSRSupport::TierNotSupported;
		}
		constexpr MeshShaderSupport ConvertMeshShaderTier(D3D12_MESH_SHADER_TIER tier)
		{
			switch (tier)
			{
			case D3D12_MESH_SHADER_TIER_NOT_SUPPORTED:
				return MeshShaderSupport::TierNotSupported;
			case D3D12_MESH_SHADER_TIER_1:
				return MeshShaderSupport::Tier1;
			}
			return MeshShaderSupport::TierNotSupported;
		}

		constexpr GfxShaderModel ConvertShaderModel(D3D_SHADER_MODEL shader_model)
		{
			switch (shader_model)
			{
			case D3D_SHADER_MODEL_6_0: return SM_6_0;
			case D3D_SHADER_MODEL_6_1: return SM_6_1;
			case D3D_SHADER_MODEL_6_2: return SM_6_2;
			case D3D_SHADER_MODEL_6_3: return SM_6_3;
			case D3D_SHADER_MODEL_6_4: return SM_6_4;
			case D3D_SHADER_MODEL_6_5: return SM_6_5;
			case D3D_SHADER_MODEL_6_6: return SM_6_6;
			case D3D_SHADER_MODEL_6_7: return SM_6_7;
			default:
				return SM_Unknown;
			}
		}
	}

	bool GfxCapabilities::Initialize(GfxDevice* gfx)
	{
		CD3DX12FeatureSupport feature_support;
		feature_support.Init(gfx->GetDevice());

		ray_tracing_support = ConvertRayTracingTier(feature_support.RaytracingTier());
		vsr_support			= ConvertVSRTier(feature_support.VariableShadingRateTier());
		mesh_shader_support = ConvertMeshShaderTier(feature_support.MeshShaderTier());
		shader_model		= ConvertShaderModel(feature_support.HighestShaderModel());

		if (shader_model < SM_6_6)
		{
			ADRIA_LOG(ERROR, "Device doesn't support Shader Model 6.6 which is required!");
			return false;
		}

		return true;
	}
}

