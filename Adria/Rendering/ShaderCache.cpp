#include <execution>
#include "ShaderCache.h"
#include "../Graphics/ShaderCompiler.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Logging/Logger.h"
#include "../Utilities/Timer.h"
#include "../Utilities/HashMap.h"
#include "../Utilities/HashSet.h"
#include "../Utilities/FileWatcher.h"

namespace fs = std::filesystem;

namespace adria
{
	char const* shaders_directory = "Resources/Shaders/";
	char const* new_shaders_directory = "Resources/NewShaders/";
	namespace
	{
		std::unique_ptr<FileWatcher> file_watcher;
		ShaderRecompiledEvent shader_recompiled_event;
		LibraryRecompiledEvent library_recompiled_event;
		HashMap<EShaderId, Shader> shader_map;
		HashMap<EShaderId, HashSet<fs::path>> dependent_files_map;

		constexpr std::string GetEntryPoint(EShaderId shader)
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
			case CS_BloomExtract:
				return "BloomExtract";
			case CS_BloomCombine:
				return "BloomCombine";
			case CS_BokehGeneration:
				return "BokehGeneration";
			case VS_Bokeh:
				return "BokehVS";
			case GS_Bokeh:
				return "BokehGS";
			case PS_Bokeh:
				return "BokehPS";
			case CS_Blur_Horizontal:
				return "BlurHorizontal";
			case CS_Blur_Vertical:
				return "BlurVertical";
			case CS_Picking:
				return "Picking";
			case VS_Ocean:
				return "OceanVS";
			case PS_Ocean:
				return "OceanPS";
			case VS_OceanLOD:
				return "OceanLOD_VS";
			case DS_OceanLOD:
				return "OceanLOD_DS";
			case HS_OceanLOD:
				return "OceanLOD_HS";
			default:
				return "main";
			}
			return "main";
		}
		constexpr EShaderStage GetShaderStage(EShaderId shader)
		{
			switch (shader)
			{
			case VS_Skybox:
			case VS_Texture:
			case VS_Solid:
			case VS_Billboard:
			case VS_Sun:
			case VS_Decals:
			case VS_GBuffer:
			case VS_ScreenQuad:
			case VS_LensFlare:
			case VS_Bokeh:
			case VS_DepthMap:
			case VS_DepthMap_Transparent:
			case VS_Ocean:
			case VS_OceanLOD:
			case VS_Particles:
				return EShaderStage::VS;
			case PS_Skybox:
			case PS_HosekWilkieSky:
			case PS_UniformColorSky:
			case PS_Texture:
			case PS_Solid:
			case PS_Decals:
			case PS_Decals_ModifyNormals:
			case PS_GBuffer:
			case PS_GBuffer_Mask:
			case PS_LightingPBR:
			case PS_LightingPBR_RayTracedShadows:
			case PS_ClusteredLightingPBR:
			case PS_Taa:
			case PS_Copy:
			case PS_Add:
			case PS_LensFlare:
			case PS_Bokeh:
			case PS_DepthMap:
			case PS_DepthMap_Transparent:
			case PS_Volumetric_Directional:
			case PS_Volumetric_Spot:
			case PS_Volumetric_Point:
			case PS_Volumetric_DirectionalCascades:
			case PS_Ocean:
			case PS_Particles:
				return EShaderStage::PS;
			case GS_LensFlare:
			case GS_Bokeh:
				return EShaderStage::GS;
			case CS_Blur_Horizontal:
			case CS_Blur_Vertical:
			case CS_BokehGeneration:
			case CS_BloomExtract:
			case CS_BloomCombine:
			case CS_InitialSpectrum:
			case CS_Phase:
			case CS_Spectrum:
			case CS_FFT_Horizontal:
			case CS_FFT_Vertical:
			case CS_OceanNormalMap:
			case CS_TiledLighting:
			case CS_ClusterBuilding:
			case CS_ClusterCulling:
			case CS_ParticleInitDeadList:
			case CS_ParticleReset:
			case CS_ParticleEmit:
			case CS_ParticleSimulate:
			case CS_ParticleBitonicSortStep:
			case CS_ParticleSort512:
			case CS_ParticleSortInner512:
			case CS_ParticleInitSortDispatchArgs:
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
				return EShaderStage::CS;
			case HS_OceanLOD:
				return EShaderStage::HS;
			case DS_OceanLOD:
				return EShaderStage::DS;
			case LIB_Shadows:
			case LIB_SoftShadows:
			case LIB_AmbientOcclusion:
			case LIB_Reflections:
				return EShaderStage::LIB;
			case ShaderId_Count:
			default:
				return EShaderStage::ShaderCount;
			}
		}
		constexpr std::string GetShaderSource(EShaderId shader)
		{
			switch (shader)
			{
			case VS_Skybox:
				return "Misc/SkyboxVS.hlsl";
			case PS_Skybox:
				return "Misc/SkyboxPS.hlsl";
			case PS_HosekWilkieSky:
				return "Misc/HosekWilkieSkyPS.hlsl";
			case PS_UniformColorSky:
				return "Misc/UniformColorSkyPS.hlsl";
			case VS_Texture:
				return "Misc/TextureVS.hlsl";
			case PS_Texture:
				return "Misc/TexturePS.hlsl";
			case VS_Solid:
				return "Misc/SolidVS.hlsl";
			case PS_Solid:
				return "Misc/SolidPS.hlsl";
			case VS_Sun:
				return "Misc/SunVS.hlsl";
			case VS_Billboard:
				return "Misc/BillboardVS.hlsl";
			case VS_Decals:
				return "Misc/DecalVS.hlsl";
			case PS_Decals:
			case PS_Decals_ModifyNormals:
				return "Misc/DecalPS.hlsl";
			case VS_GBuffer:
			case PS_GBuffer:
			case PS_GBuffer_Mask:
				return "Lighting/GBuffer.hlsl";
			case VS_ScreenQuad:
				return "Postprocess/ScreenQuadVS.hlsl";
			case PS_LightingPBR:
			case PS_LightingPBR_RayTracedShadows:
				return "Deferred/LightingPBR_PS.hlsl";
			case PS_ClusteredLightingPBR:
				return "Deferred/ClusterLightingPBR_PS.hlsl";
			case PS_Taa:
				return "Postprocess/TAA_PS.hlsl";
			case PS_Copy:
				return "Postprocess/CopyPS.hlsl";
			case PS_Add:
				return "Postprocess/AddPS.hlsl";
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
			case VS_DepthMap:
			case VS_DepthMap_Transparent:
				return "Shadows/DepthMapVS.hlsl";
			case PS_DepthMap:
			case PS_DepthMap_Transparent:
				return "Shadows/DepthMapPS.hlsl";
			case PS_Volumetric_Directional:
				return "Postprocess/VolumetricLightDirectionalPS.hlsl";
			case PS_Volumetric_DirectionalCascades:
				return "Postprocess/VolumetricLightDirectionalCascadesPS.hlsl";
			case PS_Volumetric_Spot:
				return "Postprocess/VolumetricLightSpotPS.hlsl";
			case PS_Volumetric_Point:
				return "Postprocess/VolumetricLightPointPS.hlsl";
			case CS_Blur_Horizontal:
			case CS_Blur_Vertical:
				return "Postprocess/Blur.hlsl";
			case CS_BokehGeneration:
				return "Postprocess/BokehGeneration.hlsl";
			case CS_BloomExtract:
			case CS_BloomCombine:
				return "Postprocess/Bloom.hlsl";
			case CS_InitialSpectrum:
				return "Ocean/InitialSpectrumCS.hlsl";
			case CS_Phase:
				return "Ocean/PhaseCS.hlsl";
			case CS_Spectrum:
				return "Ocean/SpectrumCS.hlsl";
			case CS_FFT_Horizontal:
				return "Ocean/FFT_horizontalCS.hlsl";
			case CS_FFT_Vertical:
				return "Ocean/FFT_verticalCS.hlsl";
			case CS_OceanNormalMap:
				return "Ocean/NormalMapCS.hlsl";
			case CS_TiledLighting:
				return "Deferred/TiledLightingCS.hlsl";
			case CS_ClusterBuilding:
				return "Deferred/ClusterBuildingCS.hlsl";
			case CS_ClusterCulling:
				return "Deferred/ClusterCullingCS.hlsl";
			case VS_Ocean:
		    case PS_Ocean:
				return "Ocean/Ocean.hlsl";
			case VS_OceanLOD:
			case HS_OceanLOD:
			case DS_OceanLOD:
				return "Ocean/OceanLOD.hlsl";
			case CS_Picking:
				return "Other/Picking.hlsl";
			case VS_Particles:
				return "Particles/ParticleVS.hlsl";
			case PS_Particles:
				return "Particles/ParticlePS.hlsl";
			case CS_ParticleInitDeadList:
				return "Particles/InitDeadListCS.hlsl";
			case CS_ParticleReset:
				return "Particles/ParticleResetCS.hlsl";
			case CS_ParticleEmit:
				return "Particles/ParticleEmitCS.hlsl";
			case CS_ParticleSimulate:
				return "Particles/ParticleSimulateCS.hlsl";
			case CS_ParticleBitonicSortStep:
				return "Particles/BitonicSortStepCS.hlsl";
			case CS_ParticleSort512:
				return "Particles/Sort512CS.hlsl";
			case CS_ParticleSortInner512:
				return "Particles/SortInner512CS.hlsl";
			case CS_ParticleInitSortDispatchArgs:
				return "Particles/InitSortDispatchArgsCS.hlsl";
			case CS_GenerateMips:
				return "Misc/GenerateMipsCS.hlsl";
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
			case CS_Ambient:
				return "Lighting/Ambient.hlsl";
			case LIB_Shadows:
			case LIB_SoftShadows:
				return "RayTracing/RayTracedShadows.hlsl";
			case LIB_AmbientOcclusion:
				return "RayTracing/RayTracedAmbientOcclusion.hlsl";
			case LIB_Reflections:
				return "RayTracing/RayTracedReflections.hlsl";
			case ShaderId_Count:
			default:
				return "";
			}
		}
		constexpr std::vector<ShaderMacro> GetShaderMacros(EShaderId shader)
		{
			switch (shader)
			{
			case PS_Decals_ModifyNormals:
				return { {L"DECAL_MODIFY_NORMALS", L""} };
			case VS_DepthMap_Transparent:
			case PS_DepthMap_Transparent:
				return { {L"TRANSPARENT", L"1"} };
			case PS_LightingPBR_RayTracedShadows:
				return { { L"RAY_TRACED_SHADOWS", L"" } };
			case LIB_SoftShadows:
				return { { L"SOFT_SHADOWS", L"" } };
			case PS_GBuffer_Mask:
				return { { L"MASK", L"1" } };
			default:
				return {};
			}
		}
		constexpr bool UseNewShadersDirectory(EShaderId shader)
		{
			switch (shader)
			{
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
			case LIB_AmbientOcclusion:
			case LIB_Reflections:
			case LIB_Shadows:
			case LIB_SoftShadows:
			case VS_GBuffer:
			case PS_GBuffer:
			case PS_GBuffer_Mask:
			case VS_LensFlare:
			case GS_LensFlare:
			case PS_LensFlare:
			case VS_Bokeh:
			case GS_Bokeh:
			case PS_Bokeh:
			case CS_BokehGeneration:
			case CS_Clouds:
			case CS_BloomExtract:
			case CS_BloomCombine:
			case CS_Blur_Horizontal:
			case CS_Blur_Vertical:
			case CS_Picking:
			case VS_Ocean:
			case PS_Ocean:
			case VS_OceanLOD:
			case HS_OceanLOD:
			case DS_OceanLOD:
				return true;
			default:
				return false;
			}
			return false;
		}

		void CompileShader(EShaderId shader)
		{
			if (shader == ShaderId_Invalid) return;

			ShaderDesc shader_desc{};
			shader_desc.entry_point = GetEntryPoint(shader);
			shader_desc.stage = GetShaderStage(shader);
			shader_desc.macros = GetShaderMacros(shader);
			shader_desc.model = SM_6_6;
			shader_desc.file = UseNewShadersDirectory(shader) ?
				std::string(new_shaders_directory) + GetShaderSource(shader) :
				std::string(shaders_directory) + GetShaderSource(shader);
#if _DEBUG
			shader_desc.flags = ShaderCompilerFlag_DisableOptimization | ShaderCompilerFlag_Debug;
#else
			shader_desc.flags = ShaderCompilerFlag_None;
#endif
			ShaderCompileOutput output;
			ShaderCompiler::CompileShader(shader_desc, output);
			shader_map[shader] = std::move(output.shader);
			dependent_files_map[shader].clear();
			dependent_files_map[shader].insert(output.dependent_files.begin(), output.dependent_files.end());
			
			shader_desc.stage == EShaderStage::LIB ?
				library_recompiled_event.Broadcast(shader) : shader_recompiled_event.Broadcast(shader);
		}
		void CompileAllShaders()
		{
			Timer t;
			ADRIA_LOG(INFO, "Compiling all shaders...");
			using UnderlyingType = std::underlying_type_t<EShaderId>;
			std::vector<UnderlyingType> shaders(ShaderId_Count);
			std::iota(std::begin(shaders), std::end(shaders), 0);
			std::for_each(
				std::execution::seq,
				std::begin(shaders),
				std::end(shaders),
				[](UnderlyingType s)
				{
					EShaderId shader = static_cast<EShaderId>(s);
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
		file_watcher->AddPathToWatch(new_shaders_directory);
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

	Shader const& ShaderCache::GetShader(EShaderId shader)
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

