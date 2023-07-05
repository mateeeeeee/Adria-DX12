#pragma once
#include "Events/Delegate.h"

namespace adria
{
	class GfxDevice;
	class GfxShader;

	enum GfxShaderID : uint8
	{
		GfxShaderID_Invalid,
		VS_Sky,
		PS_Sky,
		CS_MinimalAtmosphereSky,
		CS_HosekWilkieSky,
		VS_FullscreenQuad,
		VS_GBuffer,
		PS_GBuffer,
		PS_GBuffer_Mask,
		VS_Shadow,
		PS_Shadow,
		VS_Shadow_Transparent,
		PS_Shadow_Transparent,
		VS_LensFlare,
		GS_LensFlare,
		PS_LensFlare,
		CS_LensFlare2,
		VS_Bokeh,
		GS_Bokeh,
		PS_Bokeh,
		PS_Copy,
		PS_Add,
		VS_Sun,
		VS_Simple,
		PS_Texture,
		PS_Solid,
		CS_Blur_Horizontal,
		CS_Blur_Vertical,
		CS_BloomDownsample,
		CS_BloomDownsampleFirstPass,
		CS_BloomUpsample,
		CS_BokehGeneration,
		CS_GenerateMips,
		CS_FFT_Horizontal,
		CS_FFT_Vertical,
		CS_InitialSpectrum,
		CS_OceanNormals,
		CS_Phase,
		CS_Spectrum,
		VS_Ocean,
		PS_Ocean,
		VS_Decals,
		PS_Decals,
		PS_Decals_ModifyNormals,
		VS_OceanLOD,
		DS_OceanLOD,
		HS_OceanLOD,
		CS_Picking,
		CS_BuildHistogram,
		CS_HistogramReduction,
		CS_Exposure,
		CS_Ssao,
		CS_Hbao,
		CS_Ssr,
		CS_Fog,
		CS_Tonemap,
		CS_MotionVectors,
		CS_MotionBlur,
		CS_Dof,
		CS_Fxaa,
		CS_GodRays,
		CS_Ambient,
		CS_Clouds,
		CS_Clouds_Reprojection,
		CS_CloudDetail,
		CS_CloudShape,
		CS_CloudType,
		VS_CloudsCombine,
		PS_CloudsCombine,
		CS_Taa,
		CS_DeferredLighting,
		CS_VolumetricLighting,
		CS_TiledDeferredLighting,
		CS_ClusteredDeferredLighting,
		CS_ClusterBuilding,
		CS_ClusterCulling,
		CS_ClearCounters,
		CS_CullInstances1stPhase,
		CS_CullInstances1stPhase_NoOcclusionCull,
		CS_CullInstances2ndPhase,
		CS_BuildMeshletCullArgs1stPhase,
		CS_BuildMeshletCullArgs2ndPhase,
		CS_CullMeshlets1stPhase,
		CS_CullMeshlets1stPhase_NoOcclusionCull,
		CS_CullMeshlets2ndPhase,
		CS_BuildMeshletDrawArgs1stPhase,
		CS_BuildMeshletDrawArgs2ndPhase,
		MS_DrawMeshlets,
		PS_DrawMeshlets,
		CS_BuildInstanceCullArgs,
		CS_InitializeHZB,
		CS_HZBMips,
		CS_RTAOFilter,
		//CS_DDGIUpdateIrradiance,
		//CS_DDGIUpdateIrradianceBorder,
		//CS_DDGIUpdateDistance,
		//CS_DDGIUpdateDistanceBorder,
		//LIB_DDGIRayTracing,
		LIB_Shadows,
		LIB_AmbientOcclusion,
		LIB_Reflections,
		LIB_PathTracing,
		ShaderId_Count
	};

	DECLARE_MULTICAST_DELEGATE(ShaderRecompiledEvent, GfxShaderID);
	DECLARE_MULTICAST_DELEGATE(LibraryRecompiledEvent, GfxShaderID);

	class ShaderCache
	{
	public:
		static void Initialize();
		static void Destroy();
		static void CheckIfShadersHaveChanged();
		static ShaderRecompiledEvent& GetShaderRecompiledEvent();
		static LibraryRecompiledEvent& GetLibraryRecompiledEvent();
		static GfxShader const& GetShader(GfxShaderID shader);
	};
}