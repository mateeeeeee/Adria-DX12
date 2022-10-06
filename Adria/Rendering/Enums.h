#pragma once
#include "../Core/Definitions.h"



namespace adria
{
	
	enum EShader : uint8
	{
		EShader_Invalid,
		VS_Skybox,
		PS_Skybox,
		PS_UniformColorSky,
		PS_HosekWilkieSky,
		VS_ScreenQuad,
		PS_ToneMap,
		PS_Fxaa,
		PS_Taa,
		VS_GBufferPBR,
		PS_GBufferPBR,
		PS_GBufferPBR_Mask,
		PS_AmbientPBR,
		PS_AmbientPBR_AO,
		PS_AmbientPBR_IBL,
		PS_AmbientPBR_AO_IBL,
		PS_LightingPBR,
		PS_LightingPBR_RayTracedShadows,
		PS_ClusteredLightingPBR,
		VS_DepthMap,
		PS_DepthMap,
		VS_DepthMap_Transparent,
		PS_DepthMap_Transparent,
		PS_Volumetric_DirectionalCascades,
		PS_Volumetric_Directional,
		PS_Volumetric_Spot,
		PS_Volumetric_Point,
		PS_Ssao,
		PS_Hbao,
		PS_Ssr,
		PS_GodRays,
		PS_Dof,
		PS_VolumetricClouds,
		VS_LensFlare,
		GS_LensFlare,
		PS_LensFlare,
		VS_Bokeh,
		GS_Bokeh,
		PS_Bokeh,
		PS_VelocityBuffer,
		PS_MotionBlur,
		PS_Fog,
		PS_Copy,
		PS_Add,
		VS_Solid,
		PS_Solid,
		VS_Sun,
		VS_Billboard,
		VS_Texture,
		PS_Texture,
		CS_Blur_Horizontal,
		CS_Blur_Vertical,
		CS_BloomExtract,
		CS_BloomCombine,
		CS_TiledLighting,
		CS_ClusterBuilding,
		CS_ClusterCulling,
		CS_BokehGenerate,
		CS_GenerateMips,
		CS_FFT_Horizontal,
		CS_FFT_Vertical,
		CS_InitialSpectrum,
		CS_OceanNormalMap,
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
		VS_Particles,
		PS_Particles,
		CS_ParticleInitDeadList,
		CS_ParticleReset,
		CS_ParticleEmit,
		CS_ParticleSimulate,
		CS_ParticleBitonicSortStep,
		CS_ParticleSort512,
		CS_ParticleSortInner512,
		CS_ParticleInitSortDispatchArgs,
		CS_Picker,
		LIB_Shadows,
		LIB_SoftShadows,
		LIB_AmbientOcclusion,
		LIB_Reflections,
		LIB_PathTracing,
		CS_BuildHistogram,
		CS_HistogramReduction,
		EShader_Count
	};

	enum class ERootSignature : uint8
	{
		Invalid,
		Skybox,
		Sky,
		ToneMap,
		FXAA,
		TAA,
		GbufferPBR,
		AmbientPBR,
		LightingPBR,
		ClusteredLightingPBR,
		DepthMap,
		DepthMap_Transparent,
		Volumetric,
		Forward,
		AO,
		SSR,
		DOF,
		Bokeh,
		BokehGenerate,
		GodRays,
		LensFlare,
		Blur,
		BloomExtract,
		BloomCombine,
		Copy,
		Add,
		Clouds,
		MotionBlur,
		Fog,
		TiledLighting,
		ClusterBuilding,
		ClusterCulling,
		GenerateMips,
		VelocityBuffer,
		FFT,
		InitialSpectrum,
		OceanNormalMap,
		Phase,
		Spectrum,
		Ocean,
		OceanLOD,
		Particles_Shading,
		Particles_InitDeadList,
		Particles_Reset,
		Particles_Emit,
		Particles_Simulate,
		Particles_InitSortDispatchArgs,
		Particles_Sort,
		Picker,
		Decals,
		RayTracedShadows,
		RayTracedAmbientOcclusion,
		RayTracedReflections,
		PathTracing,
		BuildHistogram,
		HistogramReduction
	};

	enum class EPipelineState : uint8
	{
		Skybox,
		UniformColorSky,
		HosekWilkieSky,
		Texture,
		Solid,
		Sun,
		Billboard,
		GBufferPBR,
		GBufferPBR_NoCull,
		GBufferPBR_Mask,
		GBufferPBR_Mask_NoCull,
		AmbientPBR,
		AmbientPBR_AO,
		AmbientPBR_IBL,
		AmbientPBR_AO_IBL,
		LightingPBR,
		LightingPBR_RayTracedShadows,
		ClusteredLightingPBR,
		ToneMap,
		FXAA,
		TAA,
		Copy,
		Copy_AlphaBlend,
		Copy_AdditiveBlend,
		Add,
		Add_AlphaBlend,
		Add_AdditiveBlend,
		DepthMap,
		DepthMap_Transparent,
		Volumetric_Directional,
		Volumetric_DirectionalCascades,
		Volumetric_Spot,
		Volumetric_Point,
		Volumetric_Clouds,
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
		TiledLighting,
		ClusterBuilding,
		ClusterCulling,
		BokehGenerate,
		Bokeh,
		GenerateMips,
		VelocityBuffer,
		FFT_Horizontal,
		FFT_Vertical,
		InitialSpectrum,
		OceanNormalMap,
		Phase,
		Spectrum,
		Ocean,
		Ocean_Wireframe,
		OceanLOD,
		OceanLOD_Wireframe,
		Particles_Shading,
		Particles_InitDeadList,
		Particles_Reset,
		Particles_Emit,
		Particles_Simulate,
		Particles_InitSortDispatchArgs,
		Particles_BitonicSortStep,
		Particles_Sort512,
		Particles_SortInner512,
		Picker,
		Decals,
		Decals_ModifyNormals,
		Solid_Wireframe,
		BuildHistogram,
		HistogramReduction,
        Unknown
	};

	enum class EToneMap : uint8
	{
		Reinhard,
		Hable,
		Linear
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

	enum class EBokehType : uint8
	{
		Hex,
		Oct,
		Circle,
		Cross
	};

	enum class EBlendMode : uint8
	{
		None,
		AlphaBlend,
		AdditiveBlend
	};

	enum class EFogType : int32
	{
		Exponential,
		ExponentialHeight
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
