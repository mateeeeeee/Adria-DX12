#pragma once
#include "Core/CoreTypes.h"


namespace adria
{
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
		CS_DDGIUpdateIrradianceColor,
		CS_DDGIUpdateIrradianceDepth,
		CS_DDGIUpdateProbeStates,
		LIB_Shadows,
		LIB_SoftShadows,
		LIB_AmbientOcclusion,
		LIB_Reflections,
		LIB_PathTracing,
		LIB_DDGIRayTracing,
		ShaderId_Count
	};

	enum class GfxRootSignatureID : uint8
	{
		Invalid,
		Common
	};

	enum class GfxPipelineStateID : uint8
	{
		Sky,
		MinimalAtmosphereSky,
		HosekWilkieSky,
		Texture,
		Solid_Wireframe,
		Sun,
		GBuffer,
		GBuffer_NoCull,
		GBuffer_Mask,
		GBuffer_Mask_NoCull,
		Ambient,
		DeferredLighting,
		VolumetricLighting,
		TiledDeferredLighting,
		ClusteredDeferredLighting,
		ClusterBuilding,
		ClusterCulling,
		ToneMap,
		FXAA,
		TAA,
		Copy,
		Copy_AlphaBlend,
		Copy_AdditiveBlend,
		Add,
		Add_AlphaBlend,
		Add_AdditiveBlend,
		Shadow,
		Shadow_Transparent,
		SSAO,
		HBAO,
		SSR,
		GodRays,
		LensFlare,
		LensFlare2,
		DOF,
		Clouds,
		Clouds_Reprojection,
		CloudDetail,
		CloudShape,
		CloudType,
		CloudsCombine,
		Fog,
		MotionBlur,
		Blur_Horizontal,
		Blur_Vertical,
		BloomDownsample,
		BloomDownsample_FirstPass,
		BloomUpsample,
		BokehGenerate,
		Bokeh,
		GenerateMips,
		MotionVectors,
		FFT_Horizontal,
		FFT_Vertical,
		InitialSpectrum,
		OceanNormals,
		Phase,
		Spectrum,
		Ocean,
		Ocean_Wireframe,
		OceanLOD,
		OceanLOD_Wireframe,
		Picking,
		Decals,
		Decals_ModifyNormals,
		BuildHistogram,
		HistogramReduction,
		Exposure,
		ClearCounters,
		CullInstances1stPhase,
		CullInstances1stPhase_NoOcclusionCull,
		CullInstances2ndPhase,
		BuildMeshletCullArgs1stPhase,
		BuildMeshletCullArgs2ndPhase,
		CullMeshlets1stPhase,
		CullMeshlets1stPhase_NoOcclusionCull,
		CullMeshlets2ndPhase,
		BuildMeshletDrawArgs1stPhase,
		BuildMeshletDrawArgs2ndPhase,
		DrawMeshlets,
		BuildInstanceCullArgs,
		InitializeHZB,
		HZBMips,
		RTAOFilter,
		DDGIUpdateIrradianceColor,
		DDGIUpdateIrradianceDepth,
		DDGIUpdateProbeStates,
        Unknown
	};

	enum class LightType : int32
	{
		Directional,
		Point,
		Spot
	};

	enum class SkyType : uint8
	{
		Skybox,
		MinimalAtmosphere,
		HosekWilkie
	};

	enum class BlendMode : uint8
	{
		None,
		AlphaBlend,
		AdditiveBlend
	};

	enum class RenderPathType
	{
		RegularDeferred,
		TiledDeferred,
		ClusteredDeferred,
		PathTracing
	};

	enum class AmbientOcclusion : uint8
	{
		None,
		SSAO,
		HBAO,
		RTAO
	};

	enum class Reflections : uint8
	{
		None,
		SSR,
		RTR
	};

	enum class DecalType : uint8
	{
		Project_XY,
		Project_YZ,
		Project_XZ
	};

	enum AntiAliasing : uint8
	{
		AntiAliasing_None = 0x0,
		AntiAliasing_FXAA = 0x1,
		AntiAliasing_TAA = 0x2
	};

	enum class MaterialAlphaMode : uint8
	{
		Opaque,
		Blend,
		Mask
	};
}
