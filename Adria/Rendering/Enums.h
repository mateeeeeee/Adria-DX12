#pragma once
#include "../Core/Definitions.h"



namespace adria
{
	
	enum Shader : u8
	{
		VS_Skybox,
		PS_Skybox,
		VS_ScreenQuad,
		PS_ToneMap,
		PS_Fxaa,
		PS_Taa,
		VS_GBufferPBR,
		PS_GBufferPBR,
		PS_AmbientPBR,
		PS_AmbientPBR_SSAO,
		PS_LightingPBR,
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
		PS_Ssr,
		PS_GodRays,
		PS_Dof,
		PS_VolumetricClouds,
		VS_LensFlare,
		GS_LensFlare,
		PS_LensFlare,
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
		CS_TiledLighting,
		CS_ClusterBuilding,
		CS_ClusterCulling
	};

	enum class RootSig : u8
	{
		eSkybox,
		eToneMap,
		eFXAA,
		eTAA,
		eGbufferPBR,
		eAmbientPBR,
		eLightingPBR,
		eClusteredLightingPBR,
		eDepthMap,
		eDepthMap_Transparent,
		eVolumetric,
		eForward,
		eSSAO,
		eSSR,
		eDof,
		eGodRays,
		eLensFlare,
		eBlur,
		eBloomExtract,
		eCopy,
		eAdd,
		eClouds,
		eMotionBlur,
		eFog,
		eTiledLighting,
		eClusterBuilding,
		eClusterCulling
	};

	enum class PSO : u8
	{
		eSkybox,
		eTexture,
		eSolid,
		eSun,
		eBillboard,
		eGbufferPBR,
		eAmbientPBR,
		eAmbientPBR_SSAO,
		eLightingPBR,
		eClusteredLightingPBR,
		//eAmbientPBR_IBL,
		//eAmbientPBR_SSAO_IBL,
		eToneMap,
		eFXAA,
		eTAA,
		eCopy,
		eCopy_AlphaBlend,
		eCopy_AdditiveBlend,
		eAdd,
		eAdd_AlphaBlend,
		eAdd_AdditiveBlend,
		eDepthMap,
		eDepthMap_Transparent,
		eVolumetric_Directional,
		eVolumetric_DirectionalCascades,
		eVolumetric_Spot,
		eVolumetric_Point,
		eVolumetric_Clouds,
		eSSAO,
		eSSR,
		eGodRays,
		eLensFlare,
		eDof,
		eClouds,
		eFog,
		eMotionBlur,
		eBlur_Horizontal,
		eBlur_Vertical,
		eBloomExtract,
		eTiledLighting,
		eClusterBuilding,
		eClusterCulling,
		//eBokehGenerate,
		//eBokehDraw,
        eUnknown
	};



	enum class ToneMap : u8
	{
		eReinhard,
		eHable,
		eLinear
	};

	enum class LightType : i32
	{
		eDirectional,
		ePoint,
		eSpot
	};

	enum class BokehType : u8
	{
		eHex,
		eOct,
		eCircle,
		eCross
	};

	enum class BlendMode : u8
	{
		eNone,
		eAlphaBlend,
		eAdditiveBlend
	};

	enum class FogType : i32
	{
		eExponential,
		eExponentialHeight
	};

	enum AntiAliasing : u8
	{
		AntiAliasing_None = 0x0,
		AntiAliasing_FXAA = 0x1,
		AntiAliasing_TAA = 0x2
	};
}
