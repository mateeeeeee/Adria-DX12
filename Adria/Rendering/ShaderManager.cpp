#include <execution>
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"
#include "ShaderManager.h"
#include "Core/Paths.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Logging/Logger.h"
#include "Utilities/Timer.h"
#include "Utilities/FileWatcher.h"

namespace fs = std::filesystem;

namespace adria
{
	namespace
	{
		std::unique_ptr<FileWatcher> file_watcher;
		ShaderRecompiledEvent shader_recompiled_event;
		LibraryRecompiledEvent library_recompiled_event;
		std::unordered_map<GfxShaderKey, GfxShader, GfxShaderKeyHash> shader_map;
		std::unordered_map<GfxShaderKey, std::vector<fs::path>, GfxShaderKeyHash> dependent_files_map;

		constexpr GfxShaderStage GetShaderStage(ShaderID shader)
		{
			switch (shader)
			{
			case VS_Sky:
			case VS_Simple:
			case VS_Sun:
			case VS_Decals:
			case VS_GBuffer:
			case VS_FullscreenTriangle:
			case VS_LensFlare:
			case VS_Shadow:
			case VS_Ocean:
			case VS_OceanLOD:
			case VS_CloudsCombine:
			case VS_Debug:
			case VS_DDGIVisualize:
			case VS_Rain:
			case VS_RainBlocker:
				return GfxShaderStage::VS;
			case PS_Sky:
			case PS_Texture:
			case PS_Solid:
			case PS_Decals:
			case PS_GBuffer:
			case PS_Copy:
			case PS_Add:
			case PS_LensFlare:
			case PS_Shadow:
			case PS_Ocean:
			case PS_CloudsCombine:
			case PS_DrawMeshlets:
			case PS_Debug:
			case PS_DDGIVisualize:
			case PS_Rain:
			case PS_VolumetricFog_CombineFog:
			case PS_VRSOverlay:
				return GfxShaderStage::PS;
			case GS_LensFlare:
				return GfxShaderStage::GS;
			case CS_Blur_Horizontal:
			case CS_Blur_Vertical:
			case CS_BloomDownsample:
			case CS_BloomUpsample:
			case CS_InitialSpectrum:
			case CS_Phase:
			case CS_Spectrum:
			case CS_FFT_Horizontal:
			case CS_FFT_Vertical:
			case CS_OceanNormals:
			case CS_Picking:
			case CS_GenerateMips:
			case CS_BuildHistogram:
			case CS_HistogramReduction:
			case CS_Exposure:
			case CS_Ssao:
			case CS_Hbao:
			case CS_Ssr:
			case CS_ExponentialHeightFog:
			case CS_Tonemap:
			case CS_MotionVectors:
			case CS_MotionBlur:
			case CS_GodRays:
			case CS_FilmEffects:
			case CS_Fxaa:
			case CS_Ambient:
			case CS_Clouds:
			case CS_CloudShape:
			case CS_CloudDetail:
			case CS_CloudType:
			case CS_Taa:
			case CS_DeferredLighting:
			case CS_VolumetricLighting:
			case CS_TiledDeferredLighting:
			case CS_ClusteredDeferredLighting:
			case CS_ClusterBuilding:
			case CS_ClusterCulling:
			case CS_HosekWilkieSky:
			case CS_MinimalAtmosphereSky:
			case CS_LensFlare2:
			case CS_ClearCounters:
			case CS_CullInstances:
			case CS_BuildMeshletCullArgs:
			case CS_CullMeshlets:
			case CS_BuildMeshletDrawArgs:
			case CS_BuildInstanceCullArgs:
			case CS_InitializeHZB:
			case CS_HZBMips:
			case CS_RTAOFilter:
			case CS_DDGIUpdateIrradiance:
			case CS_DDGIUpdateDistance:
			case CS_RainSimulation:
			case CS_ReSTIRGI_InitialSampling:
			case CS_VolumetricFog_LightInjection:
			case CS_VolumetricFog_ScatteringIntegration:
			case CS_RendererOutput:
			case CS_DepthOfField_ComputeCoC:
			case CS_DepthOfField_ComputeSeparatedCoC:
			case CS_DepthOfField_DownsampleCoC:
			case CS_DepthOfField_ComputePrefilteredTexture:
			case CS_DepthOfField_BokehFirstPass:
			case CS_DepthOfField_BokehSecondPass:
			case CS_DepthOfField_ComputePostfilteredTexture:
			case CS_DepthOfField_Combine:
				return GfxShaderStage::CS;
			case HS_OceanLOD:
				return GfxShaderStage::HS;
			case DS_OceanLOD:
				return GfxShaderStage::DS;
			case MS_DrawMeshlets:
				return GfxShaderStage::MS;
			case LIB_Shadows:
			case LIB_AmbientOcclusion:
			case LIB_Reflections:
			case LIB_PathTracing:
			case LIB_DDGIRayTracing:
				return GfxShaderStage::LIB;
			case ShaderId_Count:
			default:
				return GfxShaderStage::ShaderStageCount;
			}
		}
		constexpr std::string GetShaderSource(ShaderID shader)
		{
			switch (shader)
			{
			case VS_Sky:
			case PS_Sky:
			case CS_HosekWilkieSky:
			case CS_MinimalAtmosphereSky:
				return "Weather/Sky.hlsl";
			case VS_Rain:
			case PS_Rain:
				return "Weather/Rain.hlsl";
			case VS_RainBlocker:
				return "Weather/RainBlocker.hlsl";
			case CS_RainSimulation:
				return "Weather/RainSimulation.hlsl";
			case VS_Simple:
			case VS_Sun:
			case PS_Texture:
			case PS_Solid:
				return "Other/Simple.hlsl";
			case VS_Debug:
			case PS_Debug:
				return "Other/Debug.hlsl";
			case VS_Decals:
			case PS_Decals:
				return "Other/Decals.hlsl";
			case VS_GBuffer:
			case PS_GBuffer:
				return "Lighting/GBuffer.hlsl";
			case VS_FullscreenTriangle:
				return "Other/FullscreenTriangle.hlsl";
			case PS_Copy:
				return "Other/CopyTexture.hlsl";
			case PS_Add:
				return "Other/AddTextures.hlsl";
			case VS_LensFlare:
			case GS_LensFlare:
			case PS_LensFlare:
				return "Postprocess/LensFlare.hlsl";
			case CS_LensFlare2:
				return "Postprocess/LensFlare2.hlsl";
			case CS_Clouds:
			case VS_CloudsCombine:
			case PS_CloudsCombine:
				return "Weather/VolumetricClouds.hlsl";
			case CS_CloudShape:
			case CS_CloudDetail:
			case CS_CloudType:
				return "Weather/CloudNoise.hlsl";
			case VS_Shadow:
			case PS_Shadow:
				return "Lighting/Shadow.hlsl";
			case CS_Blur_Horizontal:
			case CS_Blur_Vertical:
				return "Postprocess/Blur.hlsl";
			case CS_BloomDownsample:
			case CS_BloomUpsample:
				return "Postprocess/Bloom.hlsl";
			case CS_InitialSpectrum:
				return "Ocean/InitialSpectrum.hlsl";
			case CS_Phase:
				return "Ocean/Phase.hlsl";
			case CS_Spectrum:
				return "Ocean/Spectrum.hlsl";
			case CS_FFT_Horizontal:
			case CS_FFT_Vertical:
				return "Ocean/FFT.hlsl";
			case CS_OceanNormals:
				return "Ocean/OceanNormals.hlsl";
			case VS_Ocean:
		    case PS_Ocean:
				return "Ocean/Ocean.hlsl";
			case VS_OceanLOD:
			case HS_OceanLOD:
			case DS_OceanLOD:
				return "Ocean/OceanLOD.hlsl";
			case CS_Picking:
				return "Other/Picking.hlsl";
			case CS_GenerateMips:
				return "Other/GenerateMips.hlsl";
			case CS_BuildHistogram:
				return "Postprocess/AutoExposure/BuildHistogram.hlsl";
			case CS_HistogramReduction:
				return "Postprocess/AutoExposure/HistogramReduction.hlsl";
			case CS_Exposure:
				return "Postprocess/AutoExposure/Exposure.hlsl";
			case CS_Ssao:
				return "Postprocess/SSAO.hlsl";
			case CS_Hbao:
				return "Postprocess/HBAO.hlsl";
			case CS_Ssr:
				return "Postprocess/SSR.hlsl";
			case CS_ExponentialHeightFog:
				return "Postprocess/ExponentialHeightFog.hlsl";
			case CS_Tonemap:
				return "Postprocess/Tonemap.hlsl";
			case CS_MotionVectors:
				return "Postprocess/MotionVectors.hlsl";
			case CS_MotionBlur:
				return "Postprocess/MotionBlur.hlsl";
			case CS_FilmEffects:
				return "Postprocess/FilmEffects.hlsl";
			case CS_GodRays:
				return "Postprocess/GodRays.hlsl";
			case CS_Fxaa:
				return "Postprocess/FXAA.hlsl";
			case CS_Taa:
				return "Postprocess/TAA.hlsl";
			case CS_Ambient:
				return "Lighting/Ambient.hlsl";
			case CS_DeferredLighting:
				return "Lighting/DeferredLighting.hlsl";
			case CS_VolumetricLighting:
				return "Lighting/VolumetricLighting.hlsl";
			case CS_TiledDeferredLighting:
				return "Lighting/TiledDeferredLighting.hlsl";
			case CS_ClusteredDeferredLighting:
				return "Lighting/ClusteredDeferredLighting.hlsl";
			case CS_ClusterBuilding:
				return "Lighting/ClusterBuilding.hlsl";
			case CS_ClusterCulling:
				return "Lighting/ClusterCulling.hlsl";
			case CS_ClearCounters:
				return "Meshlets/ClearCounters.hlsl";
			case CS_BuildMeshletCullArgs:
			case CS_CullMeshlets:
				return "Meshlets/CullMeshlets.hlsl";
			case CS_BuildInstanceCullArgs:
			case CS_CullInstances:
				return "Meshlets/CullInstances.hlsl";
			case CS_BuildMeshletDrawArgs:
			case MS_DrawMeshlets:
			case PS_DrawMeshlets:
				return "Meshlets/DrawMeshlets.hlsl";
			case CS_InitializeHZB:
			case CS_HZBMips:
				return "Meshlets/HZB.hlsl";
			case CS_VolumetricFog_LightInjection:
			case CS_VolumetricFog_ScatteringIntegration:
			case PS_VolumetricFog_CombineFog:
				return "Lighting/VolumetricFog.hlsl";
			case CS_RTAOFilter:
				return "RayTracing/RTAOFilter.hlsl";
			case CS_DDGIUpdateIrradiance:
				return "DDGI/DDGIUpdateIrradiance.hlsl";
			case CS_DDGIUpdateDistance:
				return "DDGI/DDGIUpdateDistance.hlsl";
			case VS_DDGIVisualize:
			case PS_DDGIVisualize:
				return "DDGI/DDGIVisualize.hlsl";
			case LIB_DDGIRayTracing:
				return "DDGI/DDGIRayTrace.hlsl";
			case LIB_Shadows:
				return "RayTracing/RayTracedShadows.hlsl";
			case LIB_AmbientOcclusion:
				return "RayTracing/RayTracedAmbientOcclusion.hlsl";
			case LIB_Reflections:
				return "RayTracing/RayTracedReflections.hlsl";
			case LIB_PathTracing:
				return "RayTracing/PathTracer.hlsl";
			case CS_ReSTIRGI_InitialSampling:
				return "ReSTIR/InitialSampling.hlsl";
			case CS_RendererOutput:
				return "Other/RendererOutput.hlsl";
			case CS_DepthOfField_ComputeCoC:
			case CS_DepthOfField_ComputeSeparatedCoC:
			case CS_DepthOfField_DownsampleCoC:
				return "Postprocess/DepthOfField/CircleOfConfusion.hlsl";
			case CS_DepthOfField_ComputePrefilteredTexture:
			case CS_DepthOfField_ComputePostfilteredTexture:
			case CS_DepthOfField_Combine:
				return "Postprocess/DepthOfField/DepthOfField.hlsl";
			case CS_DepthOfField_BokehFirstPass:
			case CS_DepthOfField_BokehSecondPass:
				return "Postprocess/DepthOfField/Bokeh.hlsl";
			case PS_VRSOverlay:
				return "Other/VRSOverlay.hlsl";
			case ShaderId_Count:
			default:
				return "";
			}
		}
		constexpr std::string GetEntryPoint(ShaderID shader)
		{
			switch (shader)
			{
			case CS_ClearCounters:
				return "ClearCountersCS";
			case CS_CullMeshlets:
				return "CullMeshletsCS";
			case CS_BuildMeshletCullArgs:
				return "BuildMeshletCullArgsCS";
			case CS_CullInstances:
				return "CullInstancesCS";
			case CS_BuildInstanceCullArgs:
				return "BuildInstanceCullArgsCS";
			case CS_BuildMeshletDrawArgs:
				return "BuildMeshletDrawArgsCS";
			case MS_DrawMeshlets:
				return "DrawMeshletsMS";
			case PS_DrawMeshlets:
				return "DrawMeshletsPS";
			case CS_InitializeHZB:
				return "InitializeHZB_CS";
			case CS_HZBMips:
				return "HZBMipsCS";
			case CS_BuildHistogram:
				return "BuildHistogramCS";
			case CS_HistogramReduction:
				return "HistogramReductionCS";
			case CS_Exposure:
				return "ExposureCS";
			case CS_Ssao:
				return "SSAO_CS";
			case CS_Hbao:
				return "HBAO_CS";
			case CS_Ssr:
				return "SSR_CS";
			case CS_ExponentialHeightFog:
				return "ExponentialHeightFogCS";
			case CS_Tonemap:
				return "TonemapCS";
			case CS_MotionVectors:
				return "MotionVectorsCS";
			case CS_MotionBlur:
				return "MotionBlurCS";
			case CS_GodRays:
				return "GodRaysCS";
			case CS_FilmEffects:
				return "FilmEffectsCS";
			case CS_Fxaa:
				return "FXAA_CS";
			case CS_Ambient:
				return "AmbientCS";
			case VS_GBuffer:
				return "GBufferVS";
			case PS_GBuffer:
				return "GBufferPS";
			case VS_LensFlare:
				return "LensFlareVS";
			case GS_LensFlare:
				return "LensFlareGS";
			case PS_LensFlare:
				return "LensFlarePS";
			case VS_Debug:
				return "DebugVS";
			case PS_Debug:
				return "DebugPS";
			case CS_Clouds:
				return "CloudsCS";
			case CS_CloudShape:
				return "CloudShapeCS";
			case CS_CloudDetail:
				return "CloudDetailCS";
			case CS_CloudType:
				return "CloudTypeCS";
			case CS_BloomDownsample:
				return "BloomDownsampleCS";
			case CS_BloomUpsample:
				return "BloomUpsampleCS";
			case CS_Blur_Horizontal:
				return "Blur_HorizontalCS";
			case CS_Blur_Vertical:
				return "Blur_VerticalCS";
			case CS_Picking:
				return "PickingCS";
			case VS_Ocean:
				return "OceanVS";
			case PS_Ocean:
				return "OceanPS";
			case VS_OceanLOD:
				return "OceanVS_LOD";
			case DS_OceanLOD:
				return "OceanDS_LOD";
			case HS_OceanLOD:
				return "OceanHS_LOD";
			case CS_FFT_Horizontal:
				return "FFT_HorizontalCS";
			case CS_FFT_Vertical:
				return "FFT_VerticalCS";
			case CS_InitialSpectrum:
				return "InitialSpectrumCS";
			case CS_Spectrum:
				return "SpectrumCS";
			case CS_Phase:
				return "PhaseCS";
			case CS_OceanNormals:
				return "OceanNormalsCS";
			case VS_FullscreenTriangle:
				return "FullscreenTriangleVS";
			case PS_Add:
				return "AddTexturesPS";
			case PS_Copy:
				return "CopyTexturePS";
			case VS_Sky:
				return "SkyVS";
			case PS_Sky:
				return "SkyPS";
			case CS_HosekWilkieSky:
				return "HosekWilkieSkyCS";
			case CS_MinimalAtmosphereSky:
				return "MinimalAtmosphereSkyCS";
			case VS_Rain:
				return "RainVS";
			case PS_Rain:
				return "RainPS";
			case VS_RainBlocker:
				return "RainBlockerVS";
			case CS_RainSimulation:
				return "RainSimulationCS";
			case VS_Simple:
				return "SimpleVS";
			case VS_Sun:
				return "SunVS";
			case PS_Texture:
				return "TexturePS";
			case PS_Solid:
				return "SolidPS";
			case VS_Decals:
				return "DecalsVS";
			case PS_Decals:
				return "DecalsPS";
			case CS_GenerateMips:
				return "GenerateMipsCS";
			case CS_Taa:
				return "TAA_CS";
			case CS_DeferredLighting:
				return "DeferredLightingCS";
			case CS_VolumetricLighting:
				return "VolumetricLightingCS";
			case CS_TiledDeferredLighting:
				return "TiledDeferredLightingCS";
			case CS_ClusteredDeferredLighting:
				return "ClusteredDeferredLightingCS";
			case CS_ClusterBuilding:
				return "ClusterBuildingCS";
			case CS_ClusterCulling:
				return "ClusterCullingCS";
			case CS_VolumetricFog_LightInjection:
				return "LightInjectionCS";
			case CS_VolumetricFog_ScatteringIntegration:
				return "ScatteringIntegrationCS";
			case PS_VolumetricFog_CombineFog:
				return "CombineFogPS";
			case VS_Shadow:
				return "ShadowVS";
			case PS_Shadow:
				return "ShadowPS";
			case VS_CloudsCombine:
				return "CloudsCombineVS";
			case PS_CloudsCombine:
				return "CloudsCombinePS";
			case CS_LensFlare2:
				return "LensFlareCS";
			case CS_RTAOFilter:
				return "RTAOFilterCS";
			case CS_DDGIUpdateIrradiance:
				return "DDGI_UpdateIrradianceCS";
			case CS_DDGIUpdateDistance:
				return "DDGI_UpdateDistanceCS";
			case VS_DDGIVisualize:
				return "DDGIVisualizeVS";
			case PS_DDGIVisualize:
				return "DDGIVisualizePS";
			case CS_ReSTIRGI_InitialSampling:
				return "InitialSamplingCS";
			case CS_RendererOutput:
				return "RendererOutputCS";
			case CS_DepthOfField_ComputeCoC:
				return "ComputeCircleOfConfusionCS";
			case CS_DepthOfField_ComputeSeparatedCoC:
				return "ComputeSeparatedCircleOfConfusionCS";
			case CS_DepthOfField_DownsampleCoC:
				return "DownsampleCircleOfConfusionCS";
			case CS_DepthOfField_ComputePrefilteredTexture:
				return "ComputePrefilteredTextureCS";
			case CS_DepthOfField_BokehFirstPass:
				return "BokehFirstPassCS";
			case CS_DepthOfField_BokehSecondPass:
				return "BokehSecondPassCS";
			case CS_DepthOfField_ComputePostfilteredTexture:
				return "ComputePostfilteredTextureCS";
			case CS_DepthOfField_Combine:
				return "CombineCS";
			case PS_VRSOverlay:
				return "VRSOverlayPS";
			}
			return "main";
		}
		constexpr GfxShaderModel GetShaderModel(ShaderID shader)
		{
			return SM_6_7;
		}

		void CompileShader(GfxShaderKey const& shader, bool bypass_cache = false)
		{
			if (!shader.IsValid()) return;

			GfxShaderDesc shader_desc{};
			shader_desc.entry_point = GetEntryPoint(shader);
			shader_desc.stage = GetShaderStage(shader);
			shader_desc.model = GetShaderModel(shader);
			shader_desc.file = paths::ShaderDir + GetShaderSource(shader);
#if _DEBUG
			shader_desc.flags = ShaderCompilerFlag_DisableOptimization | ShaderCompilerFlag_Debug;
#else
			shader_desc.flags = ShaderCompilerFlag_None;
#endif
			shader_desc.defines = shader.GetDefines();

			GfxShaderCompileOutput output;
			bool compile_result = GfxShaderCompiler::CompileShader(shader_desc, output, bypass_cache);
			ADRIA_ASSERT(compile_result);
			if (!compile_result) return;

			shader_map[shader] = std::move(output.shader);

			dependent_files_map[shader].clear();
			for (auto const& include : output.includes) dependent_files_map[shader].push_back(fs::path(include));
			shader_desc.stage == GfxShaderStage::LIB ? library_recompiled_event.Broadcast(shader) : shader_recompiled_event.Broadcast(shader);
		}
		void OnShaderFileChanged(std::string const& filename)
		{
			for (auto const& [shader, files] : dependent_files_map)
			{
				for (uint64 i = 0; i < files.size(); ++i)
				{
					fs::path const& file = files[i];
					if (fs::equivalent(file, fs::path(filename))) CompileShader(shader, i != 0);
				}
			}
		}
	}

	void ShaderManager::Initialize()
	{
		file_watcher = std::make_unique<FileWatcher>();
		file_watcher->AddPathToWatch(paths::ShaderDir);
		std::ignore = file_watcher->GetFileModifiedEvent().AddStatic(OnShaderFileChanged);
		fs::create_directory(paths::ShaderCacheDir);
	}
	void ShaderManager::Destroy()
	{
		file_watcher = nullptr;
		shader_map.clear();
		dependent_files_map.clear();
	}
	void ShaderManager::CheckIfShadersHaveChanged()
	{
		file_watcher->CheckWatchedFiles();
	}

	GfxShader const& ShaderManager::GetGfxShader(GfxShaderKey const& shader_key)
	{
		if (shader_map.contains(shader_key)) return shader_map[shader_key];
		CompileShader(shader_key);
		return shader_map[shader_key];
	}

	ShaderRecompiledEvent& ShaderManager::GetShaderRecompiledEvent()
	{
		return shader_recompiled_event;
	}
	LibraryRecompiledEvent& ShaderManager::GetLibraryRecompiledEvent()
	{
		return library_recompiled_event;
	}
}

