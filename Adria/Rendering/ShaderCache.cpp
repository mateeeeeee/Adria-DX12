#include <execution>
#include "ShaderCache.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxDevice.h"
#include "Logging/Logger.h"
#include "Utilities/Timer.h"
#include "Utilities/HashMap.h"
#include "Utilities/HashSet.h"
#include "Utilities/FileWatcher.h"

namespace fs = std::filesystem;

namespace adria
{
	char const* shaders_directory = "Resources/NewShaders/";
	namespace
	{
		std::unique_ptr<FileWatcher> file_watcher;
		ShaderRecompiledEvent shader_recompiled_event;
		LibraryRecompiledEvent library_recompiled_event;
		HashMap<GfxShaderID, GfxShader> shader_map;
		HashMap<GfxShaderID, HashSet<fs::path>> dependent_files_map;

		constexpr std::string  GetEntryPoint(GfxShaderID shader)
		{
			switch (shader)
			{
			case CS_BuildHistogram:
				return "BuildHistogramCS";
			case CS_HistogramReduction:
				return "HistogramReductionCS";
			case CS_Exposure:
				return "ExposureCS";
			case CS_Ssao:
				return "SSAO";
			case CS_Hbao:
				return "HBAO";
			case CS_Ssr:
				return "SSR";
			case CS_Fog:
				return "Fog";
			case CS_Tonemap:
				return "Tonemap";
			case CS_MotionVectors:
				return "MotionVectors";
			case CS_MotionBlur:
				return "MotionBlur";
			case CS_Dof:
				return "DepthOfField";
			case CS_GodRays:
				return "GodRays";
			case CS_Fxaa:
				return "FXAA";
			case CS_Ambient:
				return "Ambient";
			case VS_GBuffer:
				return "GBufferVS";
			case PS_GBuffer:
			case PS_GBuffer_Mask:
				return "GBufferPS";
			case VS_LensFlare:
				return "LensFlareVS";
			case GS_LensFlare:
				return "LensFlareGS";
			case PS_LensFlare:
				return "LensFlarePS";
			case CS_Clouds:
				return "Clouds";
			case CS_BloomDownsample:
			case CS_BloomDownsampleFirstPass:
				return "BloomDownsample";
			case CS_BloomUpsample:
				return "BloomUpsample";
			case CS_BokehGeneration:
				return "BokehGeneration";
			case VS_Bokeh:
				return "BokehVS";
			case GS_Bokeh:
				return "BokehGS";
			case PS_Bokeh:
				return "BokehPS";
			case CS_Blur_Horizontal:
				return "Blur_Horizontal";
			case CS_Blur_Vertical:
				return "Blur_Vertical";
			case CS_Picking:
				return "Picking";
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
				return "FFT_Horizontal";
			case CS_FFT_Vertical:
				return "FFT_Vertical";
			case CS_InitialSpectrum:
				return "InitialSpectrum";
			case CS_Spectrum:
				return "Spectrum";
			case CS_Phase:
				return "Phase";
			case CS_OceanNormals:
				return "OceanNormals";
			case VS_FullscreenQuad:
				return "FullscreenQuad";
			case PS_Add:
				return "AddTextures";
			case PS_Copy:
				return "CopyTexture";
			case VS_Sky:
				return "SkyVS";
			case PS_Skybox:
				return "SkyboxPS";
			case PS_HosekWilkieSky:
				return "HosekWilkieSkyPS";
			case PS_UniformColorSky:
				return "UniformColorSkyPS";
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
			case PS_Decals_ModifyNormals:
				return "DecalsPS";
			case CS_GenerateMips:
				return "GenerateMips";
			case CS_Taa:
				return "TAA";
			case CS_DeferredLighting:
				return "DeferredLighting";
			case CS_VolumetricLighting:
				return "VolumetricLighting";
			case CS_TiledDeferredLighting:
				return "TiledDeferredLighting";
			case CS_ClusteredDeferredLighting:
				return "ClusteredDeferredLighting";
			case CS_ClusterBuilding:
				return "ClusterBuilding";
			case CS_ClusterCulling:
				return "ClusterCulling";
			case VS_Shadow:
			case VS_Shadow_Transparent:
				return "ShadowVS";
			case PS_Shadow:
			case PS_Shadow_Transparent:
				return "ShadowPS";
			default:
				return "main";
			}
			return "main";
		}
		constexpr GfxShaderStage GetShaderStage(GfxShaderID shader)
		{
			switch (shader)
			{
			case VS_Sky:
			case VS_Simple:
			case VS_Sun:
			case VS_Decals:
			case VS_GBuffer:
			case VS_FullscreenQuad:
			case VS_LensFlare:
			case VS_Bokeh:
			case VS_Shadow:
			case VS_Shadow_Transparent:
			case VS_Ocean:
			case VS_OceanLOD:
				return GfxShaderStage::VS;
			case PS_Skybox:
			case PS_HosekWilkieSky:
			case PS_UniformColorSky:
			case PS_Texture:
			case PS_Solid:
			case PS_Decals:
			case PS_Decals_ModifyNormals:
			case PS_GBuffer:
			case PS_GBuffer_Mask:
			case PS_Copy:
			case PS_Add:
			case PS_LensFlare:
			case PS_Bokeh:
			case PS_Shadow:
			case PS_Shadow_Transparent:
			case PS_Ocean:
				return GfxShaderStage::PS;
			case GS_LensFlare:
			case GS_Bokeh:
				return GfxShaderStage::GS;
			case CS_Blur_Horizontal:
			case CS_Blur_Vertical:
			case CS_BokehGeneration:
			case CS_BloomDownsample:
			case CS_BloomDownsampleFirstPass:
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
			case CS_Fog:
			case CS_Tonemap:
			case CS_MotionVectors:
			case CS_MotionBlur:
			case CS_Dof:
			case CS_GodRays:
			case CS_Fxaa:
			case CS_Ambient:
			case CS_Clouds:
			case CS_Taa:
			case CS_DeferredLighting:
			case CS_VolumetricLighting:
			case CS_TiledDeferredLighting:
			case CS_ClusteredDeferredLighting:
			case CS_ClusterBuilding:
			case CS_ClusterCulling:
				return GfxShaderStage::CS;
			case HS_OceanLOD:
				return GfxShaderStage::HS;
			case DS_OceanLOD:
				return GfxShaderStage::DS;
			case LIB_Shadows:
			case LIB_SoftShadows:
			case LIB_AmbientOcclusion:
			case LIB_Reflections:
			case LIB_PathTracing:
				return GfxShaderStage::LIB;
			case ShaderId_Count:
			default:
				return GfxShaderStage::ShaderCount;
			}
		}
		constexpr std::string  GetShaderSource(GfxShaderID shader)
		{
			switch (shader)
			{
			case VS_Sky:
			case PS_Skybox:
			case PS_HosekWilkieSky:
			case PS_UniformColorSky:
				return "Other/Sky.hlsl";
			case VS_Simple:
			case VS_Sun:
			case PS_Texture:
			case PS_Solid:
				return "Other/Simple.hlsl";
			case VS_Decals:
			case PS_Decals:
			case PS_Decals_ModifyNormals:
				return "Other/Decals.hlsl";
			case VS_GBuffer:
			case PS_GBuffer:
			case PS_GBuffer_Mask:
				return "Lighting/GBuffer.hlsl";
			case VS_FullscreenQuad:
				return "Other/FullscreenQuad.hlsl";
			case PS_Copy:
				return "Other/CopyTexture.hlsl";
			case PS_Add:
				return "Other/AddTextures.hlsl";
			case VS_LensFlare:
			case GS_LensFlare:
			case PS_LensFlare:
				return "Postprocess/LensFlare.hlsl";
			case VS_Bokeh:
			case GS_Bokeh:
			case PS_Bokeh:
				return "Postprocess/Bokeh.hlsl";
			case CS_Clouds:
				return "Postprocess/Clouds.hlsl";
			case VS_Shadow:
			case VS_Shadow_Transparent:
			case PS_Shadow:
			case PS_Shadow_Transparent:
				return "Lighting/Shadow.hlsl";
			case CS_Blur_Horizontal:
			case CS_Blur_Vertical:
				return "Postprocess/Blur.hlsl";
			case CS_BokehGeneration:
				return "Postprocess/BokehGeneration.hlsl";
			case CS_BloomDownsample:
			case CS_BloomDownsampleFirstPass:
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
				return "Exposure/BuildHistogram.hlsl";
			case CS_HistogramReduction:
				return "Exposure/HistogramReduction.hlsl";
			case CS_Exposure:
				return "Exposure/Exposure.hlsl";
			case CS_Ssao:
				return "Postprocess/SSAO.hlsl";
			case CS_Hbao:
				return "Postprocess/HBAO.hlsl";
			case CS_Ssr:
				return "Postprocess/SSR.hlsl";
			case CS_Fog:
				return "Postprocess/Fog.hlsl";
			case CS_Tonemap:
				return "Postprocess/Tonemap.hlsl";
			case CS_MotionVectors:
				return "Postprocess/MotionVectors.hlsl";
			case CS_MotionBlur:
				return "Postprocess/MotionBlur.hlsl";
			case CS_Dof:
				return "Postprocess/DepthOfField.hlsl";
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
			case LIB_Shadows:
			case LIB_SoftShadows:
				return "RayTracing/RayTracedShadows.hlsl";
			case LIB_AmbientOcclusion:
				return "RayTracing/RayTracedAmbientOcclusion.hlsl";
			case LIB_Reflections:
				return "RayTracing/RayTracedReflections.hlsl";
			case LIB_PathTracing:
				return "RayTracing/PathTracer.hlsl";
			case ShaderId_Count:
			default:
				return "";
			}
		}
		constexpr std::vector<GfxShaderMacro> GetShaderMacros(GfxShaderID shader)
		{
			switch (shader)
			{
			case PS_Decals_ModifyNormals:
				return { {"DECAL_MODIFY_NORMALS", ""} };
			case VS_Shadow_Transparent:
			case PS_Shadow_Transparent:
				return { {"ALPHA_TEST", "1"} };
			case LIB_SoftShadows:
				return { { "SOFT_SHADOWS", "" } };
			case PS_GBuffer_Mask:
				return { { "MASK", "1" } };
			case CS_BloomDownsampleFirstPass:
				return { {"FIRST_PASS", "1"} };
			default:
				return {};
			}
		}

		void CompileShader(GfxShaderID shader)
		{
			if (shader == GfxShaderID_Invalid) return;

			GfxShaderDesc shader_desc{};
			shader_desc.entry_point = GetEntryPoint(shader);
			shader_desc.stage = GetShaderStage(shader);
			shader_desc.macros = GetShaderMacros(shader);
			shader_desc.model = SM_6_6;
			shader_desc.file = std::string(shaders_directory) + GetShaderSource(shader);
#if _DEBUG
			shader_desc.flags = ShaderCompilerFlag_DisableOptimization | ShaderCompilerFlag_Debug;
#else
			shader_desc.flags = ShaderCompilerFlag_None;
#endif
			GfxShaderCompileOutput output;
			GfxShaderCompiler::CompileShader(shader_desc, output);
			shader_map[shader] = std::move(output.shader);
			dependent_files_map[shader].clear();
			dependent_files_map[shader].insert(output.includes.begin(), output.includes.end());
			
			shader_desc.stage == GfxShaderStage::LIB ?
				library_recompiled_event.Broadcast(shader) : shader_recompiled_event.Broadcast(shader);
		}
		void CompileAllShaders()
		{
			Timer t;
			ADRIA_LOG(INFO, "Compiling all shaders...");
			using UnderlyingType = std::underlying_type_t<GfxShaderID>;
			std::vector<UnderlyingType> shaders(ShaderId_Count);
			std::iota(std::begin(shaders), std::end(shaders), 0);
			std::for_each(
				std::execution::seq,
				std::begin(shaders),
				std::end(shaders),
				[](UnderlyingType s)
				{
					GfxShaderID shader = static_cast<GfxShaderID>(s);
					CompileShader(shader);
				});
			ADRIA_LOG(INFO, "Compilation done in %f seconds!", t.ElapsedInSeconds());
		}
		void OnShaderFileChanged(std::string const& filename)
		{
			for (auto const& [shader, files] : dependent_files_map)
			{
				if (files.contains(fs::path(filename))) CompileShader(shader);
			}
		}
	}

	void ShaderCache::Initialize()
	{
		file_watcher = std::make_unique<FileWatcher>();
		file_watcher->AddPathToWatch(shaders_directory);
		std::ignore = file_watcher->GetFileModifiedEvent().Add(OnShaderFileChanged);
		CompileAllShaders();
	}
	void ShaderCache::Destroy()
	{
		file_watcher = nullptr;
		shader_map.clear();
		dependent_files_map.clear();
	}
	void ShaderCache::CheckIfShadersHaveChanged()
	{
		file_watcher->CheckWatchedFiles();
	}

	GfxShader const& ShaderCache::GetShader(GfxShaderID shader)
	{
		return shader_map[shader];
	}
	ShaderRecompiledEvent& ShaderCache::GetShaderRecompiledEvent()
	{
		return shader_recompiled_event;
	}
	LibraryRecompiledEvent& ShaderCache::GetLibraryRecompiledEvent()
	{
		return library_recompiled_event;
	}
}

