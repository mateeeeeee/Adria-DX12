#include <execution>
#include "ShaderManager.h"
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
	namespace
	{
		constexpr char const* shaders_directory = "Resources/Shaders/";
		std::unique_ptr<FileWatcher> file_watcher;
		ShaderRecompiledEvent shader_recompiled_event;
		LibraryRecompiledEvent library_recompiled_event;
		HashMap<EShader, ShaderBlob> shader_map;
		HashMap<EShader, HashSet<fs::path>> dependent_files_map;

		constexpr EShaderStage GetStage(EShader shader)
		{
			switch (shader)
			{
			case VS_Skybox:
			case VS_Texture:
			case VS_Solid:
			case VS_Billboard:
			case VS_Sun:
			case VS_Decals:
			case VS_GBufferPBR:
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
			case PS_GBufferPBR:
			case PS_GBufferPBR_Mask:
			case PS_AmbientPBR:
			case PS_AmbientPBR_AO:
			case PS_AmbientPBR_IBL:
			case PS_AmbientPBR_AO_IBL:
			case PS_LightingPBR:
			case PS_LightingPBR_RayTracedShadows:
			case PS_ClusteredLightingPBR:
			case PS_ToneMap:
			case PS_Fxaa:
			case PS_Taa:
			case PS_Copy:
			case PS_Add:
			case PS_Ssao:
			case PS_Hbao:
			case PS_Ssr:
			case PS_LensFlare:
			case PS_GodRays:
			case PS_Dof:
			case PS_Bokeh:
			case PS_VolumetricClouds:
			case PS_VelocityBuffer:
			case PS_MotionBlur:
			case PS_Fog:
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
			case CS_BokehGenerate:
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
			case CS_Picker:
			case CS_GenerateMips:
				return EShaderStage::CS;
			case HS_OceanLOD:
				return EShaderStage::HS;
			case DS_OceanLOD:
				return EShaderStage::DS;
			case LIB_Shadows:
			case LIB_SoftShadows:
			case LIB_AmbientOcclusion:
			case LIB_Reflections:
			case LIB_PathTracing:
				return EShaderStage::LIB;
			case EShader_Count:
			default:
				return EShaderStage::ShaderCount;
			}
		}
		constexpr std::string GetShaderSource(EShader shader)
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
			case VS_GBufferPBR:
				return "Deferred/GBuffer_VS.hlsl";
			case PS_GBufferPBR:
			case PS_GBufferPBR_Mask:
				return "Deferred/GBuffer_PS.hlsl";
			case VS_ScreenQuad:
				return "Postprocess/ScreenQuadVS.hlsl";
			case PS_AmbientPBR:
			case PS_AmbientPBR_AO:
			case PS_AmbientPBR_IBL:
			case PS_AmbientPBR_AO_IBL:
				return "Deferred/AmbientPBR_PS.hlsl";
			case PS_LightingPBR:
			case PS_LightingPBR_RayTracedShadows:
				return "Deferred/LightingPBR_PS.hlsl";
			case PS_ClusteredLightingPBR:
				return "Deferred/ClusterLightingPBR_PS.hlsl";
			case PS_ToneMap:
				return "Postprocess/ToneMapPS.hlsl";
			case PS_Fxaa:
				return "Postprocess/FXAA_PS.hlsl";
			case PS_Taa:
				return "Postprocess/TAA_PS.hlsl";
			case PS_Copy:
				return "Postprocess/CopyPS.hlsl";
			case PS_Add:
				return "Postprocess/AddPS.hlsl";
			case PS_Ssao:
				return "Postprocess/SSAO_PS.hlsl";
			case PS_Hbao:
				return "Postprocess/HBAO_PS.hlsl";
			case PS_Ssr:
				return "Postprocess/SSR_PS.hlsl";
			case VS_LensFlare:
				return "Postprocess/LensFlareVS.hlsl";
			case GS_LensFlare:
				return "Postprocess/LensFlareGS.hlsl";
			case PS_LensFlare:
				return "Postprocess/LensFlarePS.hlsl";
			case PS_GodRays:
				return "Postprocess/GodRaysPS.hlsl";
			case PS_Dof:
				return "Postprocess/DOF_PS.hlsl";
			case VS_Bokeh:
				return "Postprocess/BokehVS.hlsl";
			case GS_Bokeh:
				return "Postprocess/BokehGS.hlsl";
			case PS_Bokeh:
				return "Postprocess/BokehPS.hlsl";
			case PS_VolumetricClouds:
				return "Postprocess/CloudsPS.hlsl";
			case PS_VelocityBuffer:
				return "Postprocess/VelocityBufferPS.hlsl";
			case PS_MotionBlur:
				return "Postprocess/MotionBlurPS.hlsl";
			case PS_Fog:
				return "Postprocess/FogPS.hlsl";
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
				return "Postprocess/BlurCS.hlsl";
			case CS_BokehGenerate:
				return "Postprocess/BokehCS.hlsl";
			case CS_BloomExtract:
				return "Postprocess/BloomExtractCS.hlsl";
			case CS_BloomCombine:
				return "Postprocess/BloomCombineCS.hlsl";
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
				return "Ocean/OceanVS.hlsl";
			case PS_Ocean:
				return "Ocean/OceanPS.hlsl";
			case VS_OceanLOD:
				return "Ocean/OceanLodVS.hlsl";
			case HS_OceanLOD:
				return "Ocean/OceanLodHS.hlsl";
			case DS_OceanLOD:
				return "Ocean/OceanLodDS.hlsl";
			case CS_Picker:
				return "Misc/PickerCS.hlsl";
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
			case LIB_Shadows:
			case LIB_SoftShadows:
				return "RayTracing/RayTracedShadows.hlsl";
			case LIB_AmbientOcclusion:
				return "RayTracing/RayTracedAmbientOcclusion.hlsl";
			case LIB_Reflections:
				return "RayTracing/RayTracedReflections.hlsl";
			case LIB_PathTracing:
				return "RayTracing/PathTracer.hlsl";
			case EShader_Count:
			default:
				return "";
			}
		}
		constexpr std::vector<ShaderMacro> GetShaderMacros(EShader shader)
		{
			switch (shader)
			{
			case PS_Decals_ModifyNormals:
				return { {L"DECAL_MODIFY_NORMALS", L""} };
			case PS_AmbientPBR_AO:
				return { {L"SSAO", L"1"} };
			case PS_AmbientPBR_IBL:
				return { {L"IBL", L"1"} };
			case PS_AmbientPBR_AO_IBL:
				return { {L"SSAO", L"1"}, {L"IBL", L"1"} };
			case VS_DepthMap_Transparent:
			case PS_DepthMap_Transparent:
				return { {L"TRANSPARENT", L"1"} };
			case CS_Blur_Vertical:
				return { { L"VERTICAL", L"1" } };
			case PS_LightingPBR_RayTracedShadows:
				return { { L"RAY_TRACED_SHADOWS", L"" } };
			case LIB_SoftShadows:
				return { { L"SOFT_SHADOWS", L"" } };
			case PS_GBufferPBR_Mask:
				return { { L"MASK", L"1" } };
			default:
				return {};
			}
		}

		void CompileShader(EShader shader)
		{
			if (shader == EShader_Invalid) return;

			ShaderCompileInput shader_info{ .entrypoint = "main" };
			shader_info.flags =
#if _DEBUG
			ShaderCompileInput::FlagDebug | ShaderCompileInput::FlagDisableOptimization;
#else
			ShaderCompileInput::FlagNone;
#endif
			shader_info.source_file = std::string(shaders_directory) + GetShaderSource(shader);
			shader_info.stage = GetStage(shader);
			shader_info.macros = GetShaderMacros(shader);
			ShaderCompileOutput output;
			ShaderCompiler::CompileShader(shader_info, output);
			shader_map[shader] = std::move(output.blob);
			dependent_files_map[shader].clear();
			dependent_files_map[shader].insert(output.dependent_files.begin(), output.dependent_files.end());
			
			shader_info.stage == EShaderStage::LIB ?
				library_recompiled_event.Broadcast(shader) :
				shader_recompiled_event.Broadcast(shader);
		}
		void CompileAllShaders()
		{
			ADRIA_LOG(INFO, "Compiling all shaders...");
			using UnderlyingType = std::underlying_type_t<EShader>;
			std::vector<UnderlyingType> shaders(EShader_Count);
			std::iota(std::begin(shaders), std::end(shaders), 0);
			std::for_each(
				std::execution::seq,
				std::begin(shaders),
				std::end(shaders),
				[](UnderlyingType s)
				{
					EShader shader = static_cast<EShader>(s);
					CompileShader(shader);
				});
			ADRIA_LOG(INFO, "Compilation done!");
		}
		void OnShaderFileChanged(std::string const& filename)
		{
			for (auto const& [shader, files] : dependent_files_map)
			{
				if (files.contains(fs::path(filename))) CompileShader(shader);
			}
		}
	}

	void ShaderManager::Initialize()
	{
		file_watcher = std::make_unique<FileWatcher>();
		file_watcher->AddPathToWatch(shaders_directory);
		std::ignore = file_watcher->GetFileModifiedEvent().Add(OnShaderFileChanged);
		CompileAllShaders();
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

	ShaderBlob const& ShaderManager::GetShader(EShader shader)
	{
		return shader_map[shader];
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

