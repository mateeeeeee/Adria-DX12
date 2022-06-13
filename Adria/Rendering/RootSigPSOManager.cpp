#include <d3d12.h>
#include <wrl.h> 
#include <memory>
#include <array>
#include <string_view>
#include <execution>
#include <filesystem>
#include <variant>
#include "RootSigPSOManager.h"
#include "../Graphics/d3dx12.h"
#include "../Graphics/ShaderUtility.h"
#include "../Logging/Logger.h"
#include "../Utilities/Timer.h"
#include "../Utilities/HashMap.h"

namespace fs = std::filesystem;
using namespace Microsoft::WRL;

namespace adria
{
	namespace
	{
		struct ShaderFileData
		{
			fs::file_time_type last_changed_timestamp;
		};

		struct RecreationData
		{
			std::vector<EShader> shaders;
			std::variant<D3D12_GRAPHICS_PIPELINE_STATE_DESC, D3D12_COMPUTE_PIPELINE_STATE_DESC> recreation_desc;
		};

		ID3D12Device* device;
		HashMap<EShader, ShaderBlob> shader_map;
		HashMap<EShader, ShaderFileData> shader_file_data_map;
		HashMap<ERootSignature, ComPtr<ID3D12RootSignature>> rs_map;
		HashMap<EPipelineStateObject, ComPtr<ID3D12PipelineState>> pso_map;
		HashMap<EPipelineStateObject, RecreationData> dependency_map;

		std::string const compiled_shaders_directory = "Resources/Compiled Shaders/";
		std::string const shaders_directory = "Resources/Shaders/";
		std::string const shaders_headers_directories[] = { "Resources/Shaders/Globals", "Resources/Shaders/Util/" };

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
			case PS_AmbientPBR:
			case PS_AmbientPBR_AO:
			case PS_AmbientPBR_IBL:
			case PS_AmbientPBR_AO_IBL:
			case PS_LightingPBR:
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
				return EShaderStage::CS;
			case HS_OceanLOD:
				return EShaderStage::HS;
			case DS_OceanLOD:
				return EShaderStage::DS;
			case EShader_Count:
			default:
				return EShaderStage::COUNT;
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
				return "Deferred/GeometryPassPBR_VS.hlsl";
			case PS_GBufferPBR:
				return "Deferred/GeometryPassPBR_PS.hlsl";
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
			default:
				return {};
			}
		}

		void AddDependency(EPipelineStateObject pso, std::vector<EShader> const& shaders, D3D12_GRAPHICS_PIPELINE_STATE_DESC const& desc)
		{
			dependency_map[pso] = { .shaders = shaders, .recreation_desc = desc };
		}
		void AddDependency(EPipelineStateObject pso, std::vector<EShader> const& shaders, D3D12_COMPUTE_PIPELINE_STATE_DESC const& desc)
		{
			dependency_map[pso] = { .shaders = shaders, .recreation_desc = desc };
		}

		void CompileAllShaders()
		{
			//compiled offline
			{
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SkyboxVS.cso", shader_map[VS_Skybox]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SkyboxPS.cso", shader_map[PS_Skybox]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/UniformColorSkyPS.cso", shader_map[PS_UniformColorSky]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/HosekWilkieSkyPS.cso", shader_map[PS_HosekWilkieSky]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TextureVS.cso", shader_map[VS_Texture]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TexturePS.cso", shader_map[PS_Texture]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SunVS.cso", shader_map[VS_Sun]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BillboardVS.cso", shader_map[VS_Billboard]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/DecalVS.cso", shader_map[VS_Decals]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/DecalPS.cso", shader_map[PS_Decals]);

				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ScreenQuadVS.cso", shader_map[VS_ScreenQuad]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SSAO_PS.cso", shader_map[PS_Ssao]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/HBAO_PS.cso", shader_map[PS_Hbao]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SSR_PS.cso", shader_map[PS_Ssr]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GodRaysPS.cso", shader_map[PS_GodRays]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ToneMapPS.cso", shader_map[PS_ToneMap]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FXAA_PS.cso", shader_map[PS_Fxaa]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TAA_PS.cso", shader_map[PS_Taa]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/CopyPS.cso", shader_map[PS_Copy]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/AddPS.cso", shader_map[PS_Add]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LensFlareVS.cso", shader_map[VS_LensFlare]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LensFlareGS.cso", shader_map[GS_LensFlare]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LensFlarePS.cso", shader_map[PS_LensFlare]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehVS.cso", shader_map[VS_Bokeh]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehGS.cso", shader_map[GS_Bokeh]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehPS.cso", shader_map[PS_Bokeh]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/DOF_PS.cso", shader_map[PS_Dof]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/MotionBlurPS.cso", shader_map[PS_MotionBlur]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FogPS.cso", shader_map[PS_Fog]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VelocityBufferPS.cso", shader_map[PS_VelocityBuffer]);

				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GeometryPassPBR_VS.cso", shader_map[VS_GBufferPBR]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GeometryPassPBR_PS.cso", shader_map[PS_GBufferPBR]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LightingPBR_PS.cso", shader_map[PS_LightingPBR]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterLightingPBR_PS.cso", shader_map[PS_ClusteredLightingPBR]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightDirectionalPS.cso", shader_map[PS_Volumetric_Directional]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightDirectionalCascadesPS.cso", shader_map[PS_Volumetric_DirectionalCascades]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightSpotPS.cso", shader_map[PS_Volumetric_Spot]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightPointPS.cso", shader_map[PS_Volumetric_Point]);

				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BloomExtractCS.cso", shader_map[CS_BloomExtract]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BloomCombineCS.cso", shader_map[CS_BloomCombine]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TiledLightingCS.cso", shader_map[CS_TiledLighting]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterBuildingCS.cso", shader_map[CS_ClusterBuilding]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterCullingCS.cso", shader_map[CS_ClusterCulling]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehCS.cso", shader_map[CS_BokehGenerate]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FFT_horizontalCS.cso", shader_map[CS_FFT_Horizontal]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FFT_verticalCS.cso", shader_map[CS_FFT_Vertical]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/InitialSpectrumCS.cso", shader_map[CS_InitialSpectrum]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/NormalMapCS.cso", shader_map[CS_OceanNormalMap]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/PhaseCS.cso", shader_map[CS_Phase]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SpectrumCS.cso", shader_map[CS_Spectrum]);

				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanVS.cso", shader_map[VS_Ocean]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanPS.cso", shader_map[PS_Ocean]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanLodVS.cso", shader_map[VS_OceanLOD]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanLodHS.cso", shader_map[HS_OceanLOD]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanLodDS.cso", shader_map[DS_OceanLOD]);

				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/PickerCS.cso", shader_map[CS_Picker]);

				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/InitDeadListCS.cso", shader_map[CS_ParticleInitDeadList]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleResetCS.cso", shader_map[CS_ParticleReset]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleEmitCS.cso", shader_map[CS_ParticleEmit]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleSimulateCS.cso", shader_map[CS_ParticleSimulate]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BitonicSortStepCS.cso", shader_map[CS_ParticleBitonicSortStep]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/Sort512CS.cso", shader_map[CS_ParticleSort512]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SortInner512CS.cso", shader_map[CS_ParticleSortInner512]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/InitSortDispatchArgsCS.cso", shader_map[CS_ParticleInitSortDispatchArgs]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleVS.cso", shader_map[VS_Particles]);
				ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticlePS.cso", shader_map[PS_Particles]);
			}
			
			//compiled runtime
			ShaderInfo shader_info{};
			shader_info.entrypoint = "main";
			shader_info.flags =
#if _DEBUG
				ShaderInfo::FLAG_DEBUG | ShaderInfo::FLAG_DISABLE_OPTIMIZATION;
#else
				ShaderInfo::FLAG_NONE;
#endif
			{
				shader_info.shadersource = shaders_directory + GetShaderSource(PS_Decals_ModifyNormals);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_Decals_ModifyNormals);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_Decals_ModifyNormals]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_AmbientPBR);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_AmbientPBR);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_AmbientPBR]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_AmbientPBR_AO);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_AmbientPBR_AO);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_AmbientPBR_AO]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_AmbientPBR_IBL);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_AmbientPBR_IBL);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_AmbientPBR_IBL]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_AmbientPBR_AO_IBL);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_AmbientPBR_AO_IBL);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_AmbientPBR_AO_IBL]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_LightingPBR_RayTracedShadows);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_LightingPBR_RayTracedShadows); 
				ShaderUtility::CompileShader(shader_info, shader_map[PS_LightingPBR_RayTracedShadows]);

				shader_info.shadersource = shaders_directory + GetShaderSource(VS_DepthMap);
				shader_info.stage = EShaderStage::VS;
				shader_info.macros = GetShaderMacros(VS_DepthMap);
				ShaderUtility::CompileShader(shader_info, shader_map[VS_DepthMap]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_DepthMap);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_DepthMap);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_DepthMap]);

				shader_info.shadersource = shaders_directory + GetShaderSource(VS_DepthMap_Transparent);
				shader_info.stage = EShaderStage::VS;
				shader_info.macros = GetShaderMacros(VS_DepthMap_Transparent);
				ShaderUtility::CompileShader(shader_info, shader_map[VS_DepthMap_Transparent]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_DepthMap_Transparent);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_DepthMap_Transparent);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_DepthMap_Transparent]);

				shader_info.shadersource = shaders_directory + GetShaderSource(CS_Blur_Horizontal);
				shader_info.stage = EShaderStage::CS;
				shader_info.macros = GetShaderMacros(CS_Blur_Horizontal);
				ShaderUtility::CompileShader(shader_info, shader_map[CS_Blur_Horizontal]);

				shader_info.shadersource = shaders_directory + GetShaderSource(CS_Blur_Vertical);
				shader_info.stage = EShaderStage::CS;
				shader_info.macros = GetShaderMacros(CS_Blur_Vertical);
				ShaderUtility::CompileShader(shader_info, shader_map[CS_Blur_Vertical]);

				shader_info.shadersource = shaders_directory + GetShaderSource(CS_GenerateMips);
				shader_info.stage = EShaderStage::CS;
				shader_info.macros = GetShaderMacros(CS_GenerateMips);
				ShaderUtility::CompileShader(shader_info, shader_map[CS_GenerateMips]);

				shader_info.shadersource = shaders_directory + GetShaderSource(PS_VolumetricClouds);
				shader_info.stage = EShaderStage::PS;
				shader_info.macros = GetShaderMacros(PS_VolumetricClouds);
				ShaderUtility::CompileShader(shader_info, shader_map[PS_VolumetricClouds]);
			}
		}
		void CreateAllRootSignatures()
		{
			//HLSL
			{
				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Skybox].GetPointer(), shader_map[PS_Skybox].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Skybox].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_HosekWilkieSky].GetPointer(), shader_map[PS_HosekWilkieSky].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Sky].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_ToneMap].GetPointer(), shader_map[PS_ToneMap].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ToneMap].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Fxaa].GetPointer(), shader_map[PS_Fxaa].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::FXAA].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Taa].GetPointer(), shader_map[PS_Taa].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::TAA].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_GBufferPBR].GetPointer(), shader_map[PS_GBufferPBR].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::GbufferPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_AmbientPBR].GetPointer(), shader_map[PS_AmbientPBR].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::AmbientPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_LightingPBR].GetPointer(), shader_map[PS_LightingPBR].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::LightingPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_ClusteredLightingPBR].GetPointer(), shader_map[PS_ClusteredLightingPBR].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ClusteredLightingPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_DepthMap].GetPointer(), shader_map[PS_DepthMap].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::DepthMap].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_DepthMap_Transparent].GetPointer(), shader_map[PS_DepthMap_Transparent].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::DepthMap_Transparent].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Volumetric_Directional].GetPointer(), shader_map[PS_Volumetric_Directional].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Volumetric].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Texture].GetPointer(), shader_map[PS_Texture].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Forward].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Ssr].GetPointer(), shader_map[PS_Ssr].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::SSR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_GodRays].GetPointer(), shader_map[PS_GodRays].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::GodRays].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_LensFlare].GetPointer(), shader_map[PS_LensFlare].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::LensFlare].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Dof].GetPointer(), shader_map[PS_Dof].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::DOF].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Add].GetPointer(), shader_map[PS_Add].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Add].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Fog].GetPointer(), shader_map[PS_Fog].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Fog].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_VolumetricClouds].GetPointer(), shader_map[PS_VolumetricClouds].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Clouds].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_MotionBlur].GetPointer(), shader_map[PS_MotionBlur].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::MotionBlur].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Bokeh].GetPointer(), shader_map[PS_Bokeh].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Bokeh].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_TiledLighting].GetPointer(), shader_map[CS_TiledLighting].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::TiledLighting].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ClusterBuilding].GetPointer(), shader_map[CS_ClusterBuilding].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ClusterBuilding].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ClusterCulling].GetPointer(), shader_map[CS_ClusterCulling].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ClusterCulling].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_BokehGenerate].GetPointer(), shader_map[CS_BokehGenerate].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::BokehGenerate].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_VelocityBuffer].GetPointer(), shader_map[PS_VelocityBuffer].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::VelocityBuffer].GetAddressOf())));

				rs_map[ERootSignature::Copy] = rs_map[ERootSignature::FXAA];

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_FFT_Horizontal].GetPointer(), shader_map[CS_FFT_Horizontal].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::FFT].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_InitialSpectrum].GetPointer(), shader_map[CS_InitialSpectrum].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::InitialSpectrum].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_OceanNormalMap].GetPointer(), shader_map[CS_OceanNormalMap].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::OceanNormalMap].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_Phase].GetPointer(), shader_map[CS_Phase].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Phase].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_Spectrum].GetPointer(), shader_map[CS_Spectrum].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Spectrum].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[VS_Ocean].GetPointer(), shader_map[VS_Ocean].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Ocean].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[VS_OceanLOD].GetPointer(), shader_map[VS_OceanLOD].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::OceanLOD].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Decals].GetPointer(), shader_map[PS_Decals].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Decals].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_Picker].GetPointer(), shader_map[CS_Picker].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Picker].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ParticleInitDeadList].GetPointer(), shader_map[CS_ParticleInitDeadList].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_InitDeadList].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ParticleReset].GetPointer(), shader_map[CS_ParticleReset].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Reset].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ParticleSimulate].GetPointer(), shader_map[CS_ParticleSimulate].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Simulate].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ParticleEmit].GetPointer(), shader_map[CS_ParticleEmit].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Emit].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Particles].GetPointer(), shader_map[PS_Particles].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Shading].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ParticleInitSortDispatchArgs].GetPointer(), shader_map[CS_ParticleInitSortDispatchArgs].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_InitSortDispatchArgs].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ParticleSort512].GetPointer(), shader_map[CS_ParticleSort512].GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Sort].GetAddressOf())));

				//ID3D12VersionedRootSignatureDeserializer* drs = nullptr;
				//D3D12CreateVersionedRootSignatureDeserializer(shader_map[PS_Add].GetPointer(), shader_map[PS_Add].GetLength(), IID_PPV_ARGS(&drs));
				//D3D12_VERSIONED_ROOT_SIGNATURE_DESC const* desc = drs->GetUnconvertedRootSignatureDesc();
			}

			D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
				feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			//in C++
			{
				//ao
				{
					std::array<CD3DX12_ROOT_PARAMETER1, 3> root_parameters{};
					CD3DX12_ROOT_PARAMETER1 root_parameter{};

					root_parameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
					root_parameters[1].InitAsConstantBufferView(5, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

					CD3DX12_DESCRIPTOR_RANGE1 range{};
					range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1, 0);
					root_parameters[2].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);

					std::array<D3D12_STATIC_SAMPLER_DESC, 2> samplers{};
					D3D12_STATIC_SAMPLER_DESC sampler{};
					sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
					sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
					sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
					sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
					sampler.MipLODBias = 0;
					sampler.MaxAnisotropy = 0;
					sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
					sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
					sampler.MinLOD = 0.0f;
					sampler.MaxLOD = D3D12_FLOAT32_MAX;
					sampler.ShaderRegister = 1;
					sampler.RegisterSpace = 0;
					sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
					samplers[0] = sampler;

					sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
					sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
					sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
					sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
					sampler.ShaderRegister = 0;
					samplers[1] = sampler;

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
					rootSignatureDesc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), (uint32)samplers.size(), samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

					ComPtr<ID3DBlob> signature;
					ComPtr<ID3DBlob> error;
					D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, feature_data.HighestVersion, &signature, &error);
					if (error) OutputDebugStringA((char*)error->GetBufferPointer());
					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::AO])));
				}

				//blur
				{
					std::array<CD3DX12_ROOT_PARAMETER1, 3> root_parameters{};
					CD3DX12_ROOT_PARAMETER1 root_parameter{};

					root_parameters[0].InitAsConstantBufferView(6, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);

					CD3DX12_DESCRIPTOR_RANGE1 srv_range{};
					srv_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
					root_parameters[1].InitAsDescriptorTable(1, &srv_range, D3D12_SHADER_VISIBILITY_ALL);

					CD3DX12_DESCRIPTOR_RANGE1 uav_range{};
					uav_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
					root_parameters[2].InitAsDescriptorTable(1, &uav_range, D3D12_SHADER_VISIBILITY_ALL);

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
					rootSignatureDesc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

					ComPtr<ID3DBlob> signature;
					ComPtr<ID3DBlob> error;
					HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, feature_data.HighestVersion, &signature, &error);
					if (error) OutputDebugStringA((char*)error->GetBufferPointer());

					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::Blur])));
				}

				//bloom 
				{
					rs_map[ERootSignature::BloomExtract] = rs_map[ERootSignature::Blur];

					std::array<CD3DX12_ROOT_PARAMETER1, 2> root_parameters{};
					CD3DX12_ROOT_PARAMETER1 root_parameter{};

					CD3DX12_DESCRIPTOR_RANGE1 srv_range{};
					srv_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
					root_parameters[0].InitAsDescriptorTable(1, &srv_range, D3D12_SHADER_VISIBILITY_ALL);

					CD3DX12_DESCRIPTOR_RANGE1 uav_range{};
					uav_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
					root_parameters[1].InitAsDescriptorTable(1, &uav_range, D3D12_SHADER_VISIBILITY_ALL);

					D3D12_STATIC_SAMPLER_DESC linear_clamp_sampler = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
						D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
					root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 1, &linear_clamp_sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

					ComPtr<ID3DBlob> signature;
					ComPtr<ID3DBlob> error;
					HRESULT hr = D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
					if (error) OutputDebugStringA((char*)error->GetBufferPointer());

					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::BloomCombine])));
				}

				//mips
				{
					D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};

					feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

					if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
					{
						feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
					}

					CD3DX12_DESCRIPTOR_RANGE1 srv_uav_ranges[2] = {};
					CD3DX12_ROOT_PARAMETER1 root_parameters[3] = {};
					srv_uav_ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
					srv_uav_ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
					root_parameters[0].InitAsConstants(2, 0);
					root_parameters[1].InitAsDescriptorTable(1, &srv_uav_ranges[0]);
					root_parameters[2].InitAsDescriptorTable(1, &srv_uav_ranges[1]);

					D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
						D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

					D3D12_STATIC_SAMPLER_DESC sampler = {};
					sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
					sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
					sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
					sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
					sampler.MipLODBias = 0.0f;
					sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
					sampler.MinLOD = 0.0f;
					sampler.MaxLOD = D3D12_FLOAT32_MAX;
					sampler.MaxAnisotropy = 0;
					sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
					sampler.ShaderRegister = 0;
					sampler.RegisterSpace = 0;
					sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
					root_signature_desc.Init_1_1(ARRAYSIZE(root_parameters), root_parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

					Microsoft::WRL::ComPtr<ID3DBlob> signature;
					Microsoft::WRL::ComPtr<ID3DBlob> error;

					BREAK_IF_FAILED(D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error));
					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::GenerateMips])));
				}
			}
		}
		void CreateAllPSOs()
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pso_desc{};
			InputLayout input_layout;
			{
				graphics_pso_desc.InputLayout = { nullptr, 0u };
				graphics_pso_desc.pRootSignature =rs_map[ERootSignature::Particles_Shading].Get();
				graphics_pso_desc.VS =shader_map[VS_Particles];
				graphics_pso_desc.PS =shader_map[PS_Particles];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Shading])));
				AddDependency(EPipelineStateObject::Particles_Shading, { VS_Particles, PS_Particles }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Skybox], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Skybox].Get();
				graphics_pso_desc.VS = shader_map[VS_Skybox];
				graphics_pso_desc.PS = shader_map[PS_Skybox];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Skybox])));
				AddDependency(EPipelineStateObject::Skybox, { VS_Skybox, PS_Skybox }, graphics_pso_desc);

				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Sky].Get();
				graphics_pso_desc.PS = shader_map[PS_UniformColorSky];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::UniformColorSky])));
				AddDependency(EPipelineStateObject::UniformColorSky, { VS_Skybox, PS_UniformColorSky }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_HosekWilkieSky];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::HosekWilkieSky])));
				AddDependency(EPipelineStateObject::HosekWilkieSky, { VS_Skybox, PS_HosekWilkieSky }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::ToneMap].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_ToneMap];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ToneMap])));
				AddDependency(EPipelineStateObject::ToneMap, { VS_ScreenQuad, PS_ToneMap }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::FXAA].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Fxaa];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FXAA])));
				AddDependency(EPipelineStateObject::FXAA, { VS_ScreenQuad, PS_Fxaa }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::TAA].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Taa];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::TAA])));
				AddDependency(EPipelineStateObject::TAA, { VS_ScreenQuad, PS_Taa }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_GBufferPBR], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::GbufferPBR].Get();
				graphics_pso_desc.VS = shader_map[VS_GBufferPBR];
				graphics_pso_desc.PS = shader_map[PS_GBufferPBR];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = static_cast<uint32>(3);
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GbufferPBR])));
				AddDependency(EPipelineStateObject::GbufferPBR, { VS_GBufferPBR, PS_GBufferPBR }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0u };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::AmbientPBR].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_AmbientPBR];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR])));
				AddDependency(EPipelineStateObject::AmbientPBR, { VS_ScreenQuad, PS_AmbientPBR }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_AmbientPBR_AO];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_AO])));
				AddDependency(EPipelineStateObject::AmbientPBR_AO, { VS_ScreenQuad, PS_AmbientPBR_AO }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_AmbientPBR_IBL];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_IBL])));
				AddDependency(EPipelineStateObject::AmbientPBR_IBL, { VS_ScreenQuad, PS_AmbientPBR_IBL }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_AmbientPBR_AO_IBL];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_AO_IBL])));
				AddDependency(EPipelineStateObject::AmbientPBR_AO_IBL, { VS_ScreenQuad, PS_AmbientPBR_AO_IBL }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0u };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::LightingPBR].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_LightingPBR];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LightingPBR])));
				AddDependency(EPipelineStateObject::LightingPBR, { VS_ScreenQuad, PS_LightingPBR }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_LightingPBR_RayTracedShadows];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LightingPBR_RayTracedShadows])));
				AddDependency(EPipelineStateObject::LightingPBR_RayTracedShadows, { VS_ScreenQuad, PS_LightingPBR_RayTracedShadows }, graphics_pso_desc);

				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::ClusteredLightingPBR].Get();
				graphics_pso_desc.PS = shader_map[PS_ClusteredLightingPBR];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusteredLightingPBR])));
				AddDependency(EPipelineStateObject::ClusteredLightingPBR, { VS_ScreenQuad, PS_ClusteredLightingPBR }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_DepthMap], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::DepthMap].Get();
				graphics_pso_desc.VS = shader_map[VS_DepthMap];
				graphics_pso_desc.PS = shader_map[PS_DepthMap];
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
				graphics_pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				graphics_pso_desc.RasterizerState.DepthBias = 7500;
				graphics_pso_desc.RasterizerState.DepthBiasClamp = 0.0f;
				graphics_pso_desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
				graphics_pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 0;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DepthMap])));
				AddDependency(EPipelineStateObject::DepthMap, { VS_DepthMap, PS_DepthMap }, graphics_pso_desc);

				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_DepthMap_Transparent], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::DepthMap_Transparent].Get();
				graphics_pso_desc.VS = shader_map[VS_DepthMap_Transparent];
				graphics_pso_desc.PS = shader_map[PS_DepthMap_Transparent];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DepthMap_Transparent])));
				AddDependency(EPipelineStateObject::DepthMap_Transparent, { VS_DepthMap_Transparent, PS_DepthMap_Transparent }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0u };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Volumetric].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Volumetric_Directional];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Directional])));
				AddDependency(EPipelineStateObject::Volumetric_Directional, { VS_ScreenQuad, PS_Volumetric_Directional }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_Volumetric_DirectionalCascades];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_DirectionalCascades])));
				AddDependency(EPipelineStateObject::Volumetric_DirectionalCascades, { VS_ScreenQuad, PS_Volumetric_DirectionalCascades }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_Volumetric_Spot];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Spot])));
				AddDependency(EPipelineStateObject::Volumetric_Spot, { VS_ScreenQuad, PS_Volumetric_Spot }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_Volumetric_Point];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Point])));
				AddDependency(EPipelineStateObject::Volumetric_Point, { VS_ScreenQuad, PS_Volumetric_Point }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Sun], input_layout);
				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Forward].Get();
				graphics_pso_desc.VS = shader_map[VS_Sun];
				graphics_pso_desc.PS = shader_map[PS_Texture];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Sun])));
				AddDependency(EPipelineStateObject::Sun, { VS_Sun, PS_Texture }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::AO].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Ssao];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::SSAO])));
				AddDependency(EPipelineStateObject::SSAO, { VS_ScreenQuad, PS_Ssao }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_Hbao];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::HBAO])));
				AddDependency(EPipelineStateObject::HBAO, { VS_ScreenQuad, PS_Hbao }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::SSR].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Ssr];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::SSR])));
				AddDependency(EPipelineStateObject::SSR, { VS_ScreenQuad, PS_Ssr }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::GodRays].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_GodRays];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GodRays])));
				AddDependency(EPipelineStateObject::GodRays, { VS_ScreenQuad, PS_GodRays }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::LensFlare].Get();
				graphics_pso_desc.VS = shader_map[VS_LensFlare];
				graphics_pso_desc.GS = shader_map[GS_LensFlare];
				graphics_pso_desc.PS = shader_map[PS_LensFlare];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LensFlare])));
				AddDependency(EPipelineStateObject::LensFlare, { VS_LensFlare, GS_LensFlare, PS_LensFlare }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Copy].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Copy];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy])));
				AddDependency(EPipelineStateObject::Copy, { VS_ScreenQuad, PS_Copy }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy_AlphaBlend])));
				AddDependency(EPipelineStateObject::Copy_AlphaBlend, { VS_ScreenQuad, PS_Copy }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy_AdditiveBlend])));
				AddDependency(EPipelineStateObject::Copy_AdditiveBlend, { VS_ScreenQuad, PS_Copy }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Add].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Add];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add])));
				AddDependency(EPipelineStateObject::Add, { VS_ScreenQuad, PS_Add }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add_AlphaBlend])));
				AddDependency(EPipelineStateObject::Add_AlphaBlend, { VS_ScreenQuad, PS_Add }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add_AdditiveBlend])));
				AddDependency(EPipelineStateObject::Add_AdditiveBlend, { VS_ScreenQuad, PS_Add }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Fog].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Fog];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Fog])));
				AddDependency(EPipelineStateObject::Fog, { VS_ScreenQuad, PS_Fog }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::DOF].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_Dof];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DOF])));
				AddDependency(EPipelineStateObject::DOF, { VS_ScreenQuad, PS_Dof }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Bokeh].Get();
				graphics_pso_desc.VS = shader_map[VS_Bokeh];
				graphics_pso_desc.GS = shader_map[GS_Bokeh];
				graphics_pso_desc.PS = shader_map[PS_Bokeh];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Bokeh])));
				AddDependency(EPipelineStateObject::Bokeh, { VS_Bokeh, GS_Bokeh, PS_Bokeh }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Clouds].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_VolumetricClouds];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Clouds])));
				AddDependency(EPipelineStateObject::Clouds, { VS_ScreenQuad, PS_VolumetricClouds }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::MotionBlur].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_MotionBlur];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::MotionBlur])));
				AddDependency(EPipelineStateObject::MotionBlur, { VS_ScreenQuad, PS_MotionBlur }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::VelocityBuffer].Get();
				graphics_pso_desc.VS = shader_map[VS_ScreenQuad];
				graphics_pso_desc.PS = shader_map[PS_VelocityBuffer];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::VelocityBuffer])));
				AddDependency(EPipelineStateObject::VelocityBuffer, { VS_ScreenQuad, PS_VelocityBuffer }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Ocean], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Ocean].Get();
				graphics_pso_desc.VS = shader_map[VS_Ocean];
				graphics_pso_desc.PS = shader_map[PS_Ocean];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Ocean])));
				AddDependency(EPipelineStateObject::Ocean, { VS_Ocean, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Ocean_Wireframe])));
				AddDependency(EPipelineStateObject::Ocean_Wireframe, { VS_Ocean, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::OceanLOD].Get();
				graphics_pso_desc.VS = shader_map[VS_OceanLOD];
				graphics_pso_desc.DS = shader_map[DS_OceanLOD];
				graphics_pso_desc.HS = shader_map[HS_OceanLOD];
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanLOD_Wireframe])));
				AddDependency(EPipelineStateObject::OceanLOD_Wireframe, { VS_OceanLOD, DS_OceanLOD, HS_OceanLOD, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanLOD])));
				AddDependency(EPipelineStateObject::OceanLOD, { VS_OceanLOD, DS_OceanLOD, HS_OceanLOD, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Decals], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Decals].Get();
				graphics_pso_desc.VS = shader_map[VS_Decals];
				graphics_pso_desc.PS = shader_map[PS_Decals];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Decals])));
				AddDependency(EPipelineStateObject::Decals, { VS_Decals, PS_Decals }, graphics_pso_desc);

				graphics_pso_desc.PS = shader_map[PS_Decals_ModifyNormals];
				graphics_pso_desc.NumRenderTargets = 2;
				graphics_pso_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Decals_ModifyNormals])));
				AddDependency(EPipelineStateObject::Decals_ModifyNormals, { VS_Decals, PS_Decals }, graphics_pso_desc);
			}

			D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc{};
			{
				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Picker].Get();
				compute_pso_desc.CS = shader_map[CS_Picker];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(pso_map[EPipelineStateObject::Picker].GetAddressOf())));
				AddDependency(EPipelineStateObject::Picker, { CS_Picker }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_InitDeadList].Get();
				compute_pso_desc.CS = shader_map[CS_ParticleInitDeadList];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_InitDeadList])));
				AddDependency(EPipelineStateObject::Particles_InitDeadList, { CS_ParticleInitDeadList }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Reset].Get();
				compute_pso_desc.CS = shader_map[CS_ParticleReset];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Reset])));
				AddDependency(EPipelineStateObject::Particles_Reset, { CS_ParticleReset }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Simulate].Get();
				compute_pso_desc.CS = shader_map[CS_ParticleSimulate];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Simulate])));
				AddDependency(EPipelineStateObject::Particles_Simulate, { CS_ParticleSimulate }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Emit].Get();
				compute_pso_desc.CS = shader_map[CS_ParticleEmit];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Emit])));
				AddDependency(EPipelineStateObject::Particles_Emit, { CS_ParticleEmit }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_InitSortDispatchArgs].Get();
				compute_pso_desc.CS = shader_map[CS_ParticleInitSortDispatchArgs];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_InitSortDispatchArgs])));
				AddDependency(EPipelineStateObject::Particles_InitSortDispatchArgs, { CS_ParticleInitSortDispatchArgs }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Sort].Get();
				compute_pso_desc.CS = shader_map[CS_ParticleBitonicSortStep];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_BitonicSortStep])));
				AddDependency(EPipelineStateObject::Particles_BitonicSortStep, { CS_ParticleBitonicSortStep }, compute_pso_desc);

				compute_pso_desc.CS = shader_map[CS_ParticleSort512];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Sort512])));
				AddDependency(EPipelineStateObject::Particles_Sort512, { CS_ParticleSort512 }, compute_pso_desc);

				compute_pso_desc.CS = shader_map[CS_ParticleSortInner512];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_SortInner512])));
				AddDependency(EPipelineStateObject::Particles_SortInner512, { CS_ParticleSortInner512 }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::ClusterBuilding].Get();
				compute_pso_desc.CS = shader_map[CS_ClusterBuilding];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusterBuilding])));
				AddDependency(EPipelineStateObject::ClusterBuilding, { CS_ClusterBuilding }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::ClusterCulling].Get();
				compute_pso_desc.CS = shader_map[CS_ClusterCulling];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusterCulling])));
				AddDependency(EPipelineStateObject::ClusterCulling, { CS_ClusterCulling }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::TiledLighting].Get();
				compute_pso_desc.CS = shader_map[CS_TiledLighting];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::TiledLighting])));
				AddDependency(EPipelineStateObject::TiledLighting, { CS_TiledLighting }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Blur].Get();
				compute_pso_desc.CS = shader_map[CS_Blur_Horizontal];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Blur_Horizontal])));
				AddDependency(EPipelineStateObject::Blur_Horizontal, { CS_Blur_Horizontal }, compute_pso_desc);

				compute_pso_desc.CS = shader_map[CS_Blur_Vertical];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Blur_Vertical])));
				AddDependency(EPipelineStateObject::Blur_Vertical, { CS_Blur_Vertical }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::BloomExtract].Get();
				compute_pso_desc.CS = shader_map[CS_BloomExtract];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BloomExtract])));
				AddDependency(EPipelineStateObject::BloomExtract, { CS_BloomExtract }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::BloomCombine].Get();
				compute_pso_desc.CS = shader_map[CS_BloomCombine];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BloomCombine])));
				AddDependency(EPipelineStateObject::BloomCombine, { CS_BloomCombine }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::GenerateMips].Get();
				compute_pso_desc.CS = shader_map[CS_GenerateMips];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GenerateMips])));
				AddDependency(EPipelineStateObject::GenerateMips, { CS_GenerateMips }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::BokehGenerate].Get();
				compute_pso_desc.CS = shader_map[CS_BokehGenerate];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BokehGenerate])));
				AddDependency(EPipelineStateObject::BokehGenerate, { CS_BokehGenerate }, compute_pso_desc);
				///////////////
				compute_pso_desc.pRootSignature = rs_map[ERootSignature::FFT].Get();
				compute_pso_desc.CS = shader_map[CS_FFT_Horizontal];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FFT_Horizontal])));
				AddDependency(EPipelineStateObject::FFT_Horizontal, { CS_FFT_Horizontal }, compute_pso_desc);

				compute_pso_desc.CS = shader_map[CS_FFT_Vertical];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FFT_Vertical])));
				AddDependency(EPipelineStateObject::FFT_Vertical, { CS_FFT_Vertical }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::InitialSpectrum].Get();
				compute_pso_desc.CS = shader_map[CS_InitialSpectrum];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::InitialSpectrum])));
				AddDependency(EPipelineStateObject::InitialSpectrum, { CS_InitialSpectrum }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::InitialSpectrum].Get();
				compute_pso_desc.CS = shader_map[CS_InitialSpectrum];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::InitialSpectrum])));
				AddDependency(EPipelineStateObject::InitialSpectrum, { CS_InitialSpectrum }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::OceanNormalMap].Get();
				compute_pso_desc.CS = shader_map[CS_OceanNormalMap];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanNormalMap])));
				AddDependency(EPipelineStateObject::OceanNormalMap, { CS_OceanNormalMap }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Phase].Get();
				compute_pso_desc.CS = shader_map[CS_Phase];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Phase])));
				AddDependency(EPipelineStateObject::Phase, { CS_Phase }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Spectrum].Get();
				compute_pso_desc.CS = shader_map[CS_Spectrum];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Spectrum])));
				AddDependency(EPipelineStateObject::Spectrum, { CS_Spectrum }, compute_pso_desc);
			}
		}

		void FillShaderDataFileMap()
		{
			using UnderlyingType = std::underlying_type_t<EShader>;
			for (UnderlyingType i = 0; i < EShader_Count; ++i)
			{
				ShaderFileData file_data{
					.last_changed_timestamp = std::filesystem::last_write_time(fs::path(shaders_directory + GetShaderSource((EShader)i)))
				};
				shader_file_data_map[(EShader)i] = file_data;
			}
		}
		bool HasShaderChanged(EShader shader)
		{
			std::string shader_source = GetShaderSource(shader);
			fs::file_time_type curr_timestamp = std::filesystem::last_write_time(fs::path(shaders_directory + GetShaderSource(shader)));
			bool has_changed = shader_file_data_map[shader].last_changed_timestamp != curr_timestamp;
			if (has_changed) shader_file_data_map[shader].last_changed_timestamp = curr_timestamp;
			return has_changed;
		}
		void RecreateDependentPSOs(EShader shader)
		{
			for (auto& [pso, recreation_data] : dependency_map)
			{
				if (auto it = std::find(std::begin(recreation_data.shaders), std::end(recreation_data.shaders), shader); it != std::end(recreation_data.shaders))
				{
					size_t const i = recreation_data.recreation_desc.index();
					if (i == 0)
					{
						D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = const_cast<D3D12_GRAPHICS_PIPELINE_STATE_DESC&>(std::get<0>(recreation_data.recreation_desc));
						InputLayout il;
						for (EShader shader : recreation_data.shaders)
						{
							switch (GetStage(shader))
							{
							case EShaderStage::VS:
								ShaderUtility::CreateInputLayoutWithReflection(shader_map[shader], il);
								desc.VS = shader_map[shader];
								break;
							case EShaderStage::PS:
								desc.PS = shader_map[shader];
								break;
							case EShaderStage::DS:
								desc.DS = shader_map[shader];
								break;
							case EShaderStage::HS:
								desc.HS = shader_map[shader];
								break;
							case EShaderStage::GS:
								desc.GS = shader_map[shader];
								break;
							case EShaderStage::CS:
							case EShaderStage::COUNT:
							default:
								ADRIA_ASSERT(false && "Invalid shader stage for graphics pso!");
							}
						}
						desc.InputLayout = il;
						device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso_map[pso]));
					}
					else if (i == 1)
					{
						D3D12_COMPUTE_PIPELINE_STATE_DESC& desc = const_cast<D3D12_COMPUTE_PIPELINE_STATE_DESC&>(std::get<1>(recreation_data.recreation_desc));
						ADRIA_ASSERT(recreation_data.shaders.size() == 1);
						ADRIA_ASSERT(GetStage(recreation_data.shaders[0]) == EShaderStage::CS);
						desc.CS = shader_map[shader];
						device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso_map[pso]));
					}

				}
			}
		}
	}


	void RootSigPSOManager::Initialize(ID3D12Device* _device)
	{
		device = _device;
		CompileAllShaders();
		FillShaderDataFileMap();
		CreateAllRootSignatures();
		CreateAllPSOs();
	}

	void RootSigPSOManager::Destroy()
	{
		shader_map.clear();
		shader_file_data_map.clear();
		rs_map.clear();
		pso_map.clear();
		dependency_map.clear();
		device = nullptr;
	}

	ID3D12RootSignature* RootSigPSOManager::GetRootSignature(ERootSignature root_sig)
	{
		return rs_map[root_sig].Get();
	}

	ID3D12PipelineState* RootSigPSOManager::GetPipelineState(EPipelineStateObject pso)
	{
		return pso_map[pso].Get();
	}

	void RootSigPSOManager::RecompileShader(EShader shader, bool recreate_psos /*= true*/)
	{
		ShaderInfo shader_info{ .entrypoint = "main" };
		shader_info.flags =
#if _DEBUG
			ShaderInfo::FLAG_DEBUG | ShaderInfo::FLAG_DISABLE_OPTIMIZATION;
#else
			ShaderInfo::FLAG_NONE;
#endif
		shader_info.shadersource = std::string(shaders_directory) + GetShaderSource(shader);
		shader_info.stage = GetStage(shader);
		shader_info.macros = GetShaderMacros(shader);

		ShaderUtility::CompileShader(shader_info, shader_map[shader]);
		if (recreate_psos) RecreateDependentPSOs(shader);
	}

	void RootSigPSOManager::RecompileChangedShaders()
	{
		ADRIA_LOG(INFO, "Recompiling changed shaders...");
		using UnderlyingType = std::underlying_type_t<EShader>;
		std::vector<UnderlyingType> shaders(EShader_Count);
		std::iota(std::begin(shaders), std::end(shaders), 0);
		std::for_each(
			std::execution::seq,
			std::begin(shaders),
			std::end(shaders),
			[](UnderlyingType s)
			{
				if (HasShaderChanged((EShader)s))
				{
					RecompileShader((EShader)s, false);
					RecreateDependentPSOs((EShader)s);
				}
			});
		ADRIA_LOG(INFO, "Compilation done!");
	}

	void RootSigPSOManager::RecompileAllShaders()
	{
		ADRIA_LOG(INFO, "Recompiling changed shaders...");
		using UnderlyingType = std::underlying_type_t<EShader>;
		std::vector<UnderlyingType> shaders(EShader_Count);
		std::iota(std::begin(shaders), std::end(shaders), 0);
		std::for_each(
			std::execution::seq,
			std::begin(shaders),
			std::end(shaders),
			[](UnderlyingType s)
			{
				if (HasShaderChanged((EShader)s))
				{
					RecompileShader((EShader)s, false);
				}
			});
		CreateAllPSOs();
		ADRIA_LOG(INFO, "Compilation done!");
	}
}

