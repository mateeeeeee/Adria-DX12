#pragma once
#include "../Core/Definitions.h"


namespace adria
{
	enum EShaderId : uint8
	{
		ShaderId_Invalid,
		VS_Sky,
		PS_Skybox,
		PS_UniformColorSky,
		PS_HosekWilkieSky,
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
		CS_BloomExtract,
		CS_BloomCombine,
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
		CS_Taa,
		CS_DeferredLighting,
		CS_VolumetricLighting,
		CS_TiledDeferredLighting,
		CS_ClusteredDeferredLighting,
		CS_ClusterBuilding,
		CS_ClusterCulling,
		LIB_Shadows,
		LIB_SoftShadows,
		LIB_AmbientOcclusion,
		LIB_Reflections,
		LIB_PathTracing,
		ShaderId_Count
	};

	enum class ERootSignature : uint8
	{
		Invalid,
		Common
	};

	enum class EPipelineState : uint8
	{
		Skybox,
		UniformColorSky,
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
		DOF,
		Clouds,
		Fog,
		MotionBlur,
		Blur_Horizontal,
		Blur_Vertical,
		BloomExtract,
		BloomCombine,
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
        Unknown
	};

	enum class ELightType : int32
	{
		Directional,
		Point,
		Spot
	};

	enum class ESkyType : uint8
	{
		Skybox,
		UniformColor,
		HosekWilkie
	};

	enum class EBlendMode : uint8
	{
		None,
		AlphaBlend,
		AdditiveBlend
	};

	enum class ERenderPathType
	{
		RegularDeferred,
		TiledDeferred,
		ClusteredDeferred,
		PathTracing
	};

	enum class EAmbientOcclusion : uint8
	{
		None,
		SSAO,
		HBAO,
		RTAO
	};

	enum class EReflections : uint8
	{
		None,
		SSR,
		RTR
	};

	enum class EDecalType : uint8
	{
		Project_XY,
		Project_YZ,
		Project_XZ
	};

	enum EAntiAliasing : uint8
	{
		AntiAliasing_None = 0x0,
		AntiAliasing_FXAA = 0x1,
		AntiAliasing_TAA = 0x2
	};

	enum class EMaterialAlphaMode : uint8
	{
		Opaque,
		Blend,
		Mask
	};
}
