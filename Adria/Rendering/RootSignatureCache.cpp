#include "RootSignatureCache.h"
#include "ShaderManager.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Graphics/Shader.h"
#include "../Utilities/HashMap.h"

using namespace Microsoft::WRL;

namespace adria
{
	namespace
	{
		HashMap<ERootSignature, ComPtr<ID3D12RootSignature>> rs_map;
		inline ShaderBlob const& GetShader(EShader shader)
		{
			return ShaderManager::GetShader(shader);
		}
		void CreateAllRootSignatures(ID3D12Device* device)
		{
			{
				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Skybox).GetPointer(), GetShader(PS_Skybox).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Skybox].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_HosekWilkieSky).GetPointer(), GetShader(PS_HosekWilkieSky).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Sky].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_ToneMap).GetPointer(), GetShader(PS_ToneMap).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ToneMap].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Fxaa).GetPointer(), GetShader(PS_Fxaa).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::FXAA].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Taa).GetPointer(), GetShader(PS_Taa).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::TAA].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_GBufferPBR).GetPointer(), GetShader(PS_GBufferPBR).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::GbufferPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_AmbientPBR).GetPointer(), GetShader(PS_AmbientPBR).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::AmbientPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_LightingPBR).GetPointer(), GetShader(PS_LightingPBR).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::LightingPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_ClusteredLightingPBR).GetPointer(), GetShader(PS_ClusteredLightingPBR).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ClusteredLightingPBR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_DepthMap).GetPointer(), GetShader(PS_DepthMap).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::DepthMap].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_DepthMap_Transparent).GetPointer(), GetShader(PS_DepthMap_Transparent).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::DepthMap_Transparent].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Volumetric_Directional).GetPointer(), GetShader(PS_Volumetric_Directional).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Volumetric].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Texture).GetPointer(), GetShader(PS_Texture).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Forward].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Ssr).GetPointer(), GetShader(PS_Ssr).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::SSR].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_GodRays).GetPointer(), GetShader(PS_GodRays).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::GodRays].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_LensFlare).GetPointer(), GetShader(PS_LensFlare).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::LensFlare].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Dof).GetPointer(), GetShader(PS_Dof).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::DOF].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Add).GetPointer(), GetShader(PS_Add).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Add].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Fog).GetPointer(), GetShader(PS_Fog).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Fog].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_VolumetricClouds).GetPointer(), GetShader(PS_VolumetricClouds).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Clouds].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_MotionBlur).GetPointer(), GetShader(PS_MotionBlur).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::MotionBlur].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Bokeh).GetPointer(), GetShader(PS_Bokeh).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Bokeh].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_TiledLighting).GetPointer(), GetShader(CS_TiledLighting).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::TiledLighting].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ClusterBuilding).GetPointer(), GetShader(CS_ClusterBuilding).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ClusterBuilding].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ClusterCulling).GetPointer(), GetShader(CS_ClusterCulling).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::ClusterCulling].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_BokehGenerate).GetPointer(), GetShader(CS_BokehGenerate).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::BokehGenerate].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_VelocityBuffer).GetPointer(), GetShader(PS_VelocityBuffer).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::VelocityBuffer].GetAddressOf())));

				rs_map[ERootSignature::Copy] = rs_map[ERootSignature::FXAA];

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_FFT_Horizontal).GetPointer(), GetShader(CS_FFT_Horizontal).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::FFT].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_InitialSpectrum).GetPointer(), GetShader(CS_InitialSpectrum).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::InitialSpectrum].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_OceanNormalMap).GetPointer(), GetShader(CS_OceanNormalMap).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::OceanNormalMap].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_Phase).GetPointer(), GetShader(CS_Phase).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Phase].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_Spectrum).GetPointer(), GetShader(CS_Spectrum).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Spectrum].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(VS_Ocean).GetPointer(), GetShader(VS_Ocean).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Ocean].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(VS_OceanLOD).GetPointer(), GetShader(VS_OceanLOD).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::OceanLOD].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Decals).GetPointer(), GetShader(PS_Decals).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Decals].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_Picker).GetPointer(), GetShader(CS_Picker).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Picker].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ParticleInitDeadList).GetPointer(), GetShader(CS_ParticleInitDeadList).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_InitDeadList].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ParticleReset).GetPointer(), GetShader(CS_ParticleReset).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Reset].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ParticleSimulate).GetPointer(), GetShader(CS_ParticleSimulate).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Simulate].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ParticleEmit).GetPointer(), GetShader(CS_ParticleEmit).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Emit].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Particles).GetPointer(), GetShader(PS_Particles).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Shading].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ParticleInitSortDispatchArgs).GetPointer(), GetShader(CS_ParticleInitSortDispatchArgs).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_InitSortDispatchArgs].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ParticleSort512).GetPointer(), GetShader(CS_ParticleSort512).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Particles_Sort].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_BuildHistogram).GetPointer(), GetShader(CS_BuildHistogram).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::BuildHistogram].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_HistogramReduction).GetPointer(), GetShader(CS_HistogramReduction).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::HistogramReduction].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_Exposure).GetPointer(), GetShader(CS_Exposure).GetLength(),
					IID_PPV_ARGS(rs_map[ERootSignature::Exposure].GetAddressOf())));
				//ID3D12VersionedRootSignatureDeserializer* drs = nullptr;
				//D3D12CreateVersionedRootSignatureDeserializer(GetShader(PS_Add).GetPointer(), shader_map[PS_Add].GetLength(), IID_PPV_ARGS(&drs));
				//D3D12_VERSIONED_ROOT_SIGNATURE_DESC const* desc = drs->GetUnconvertedRootSignatureDesc();
			}

			D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
				feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			
			{
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

				{
					std::array<CD3DX12_ROOT_PARAMETER1, 6> root_parameters{};
					root_parameters[0].InitAsConstantBufferView(0);
					root_parameters[1].InitAsConstantBufferView(2);
					root_parameters[2].InitAsConstantBufferView(10);
					root_parameters[3].InitAsShaderResourceView(0);

					D3D12_DESCRIPTOR_RANGE1 srv_range = {};
					srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					srv_range.NumDescriptors = 1;
					srv_range.BaseShaderRegister = 1;
					srv_range.RegisterSpace = 0;
					srv_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[4].InitAsDescriptorTable(1, &srv_range);

					D3D12_DESCRIPTOR_RANGE1 uav_range = {};
					uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					uav_range.NumDescriptors = 1;
					uav_range.BaseShaderRegister = 0;
					uav_range.RegisterSpace = 0;
					uav_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[5].InitAsDescriptorTable(1, &uav_range);

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
					root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 0, nullptr);

					Microsoft::WRL::ComPtr<ID3DBlob> signature;
					Microsoft::WRL::ComPtr<ID3DBlob> error;
					D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
					if (error)
					{
						ADRIA_ASSERT(FALSE);
					}
					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::RayTracedShadows])));
				}

				{
					std::array<CD3DX12_ROOT_PARAMETER1, 5> root_parameters{};
					root_parameters[0].InitAsConstantBufferView(0);
					root_parameters[1].InitAsConstantBufferView(10);
					root_parameters[2].InitAsShaderResourceView(0);

					D3D12_DESCRIPTOR_RANGE1 srv_range = {};
					srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					srv_range.NumDescriptors = 2;
					srv_range.BaseShaderRegister = 1;
					srv_range.RegisterSpace = 0;
					srv_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[3].InitAsDescriptorTable(1, &srv_range);

					D3D12_DESCRIPTOR_RANGE1 uav_range = {};
					uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					uav_range.NumDescriptors = 1;
					uav_range.BaseShaderRegister = 0;
					uav_range.RegisterSpace = 0;
					uav_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[4].InitAsDescriptorTable(1, &uav_range);

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
					root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 0, nullptr);

					Microsoft::WRL::ComPtr<ID3DBlob> signature;
					Microsoft::WRL::ComPtr<ID3DBlob> error;
					D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
					if (error)
					{
						ADRIA_ASSERT(FALSE);
					}
					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::RayTracedAmbientOcclusion])));
				}

				{
					std::array<CD3DX12_ROOT_PARAMETER1, 7> root_parameters{};
					root_parameters[0].InitAsConstantBufferView(0);
					root_parameters[1].InitAsConstantBufferView(10);
					root_parameters[2].InitAsShaderResourceView(0);

					D3D12_DESCRIPTOR_RANGE1 srv_range{};
					srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					srv_range.NumDescriptors = 2;
					srv_range.BaseShaderRegister = 1;
					srv_range.RegisterSpace = 0;
					srv_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[3].InitAsDescriptorTable(1, &srv_range);

					D3D12_DESCRIPTOR_RANGE1 uav_range{};
					uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					uav_range.NumDescriptors = 1;
					uav_range.BaseShaderRegister = 0;
					uav_range.RegisterSpace = 0;
					uav_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[4].InitAsDescriptorTable(1, &uav_range);

					D3D12_DESCRIPTOR_RANGE1 unbounded_srv_range{};
					unbounded_srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					unbounded_srv_range.NumDescriptors = UINT_MAX;
					unbounded_srv_range.BaseShaderRegister = 0;
					unbounded_srv_range.RegisterSpace = 1;
					unbounded_srv_range.OffsetInDescriptorsFromTableStart = 0;
					unbounded_srv_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
					root_parameters[5].InitAsDescriptorTable(1, &unbounded_srv_range);

					D3D12_DESCRIPTOR_RANGE1 geometry_srv_range{};
					geometry_srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					geometry_srv_range.NumDescriptors = 3;
					geometry_srv_range.BaseShaderRegister = 0;
					geometry_srv_range.RegisterSpace = 2;
					geometry_srv_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[6].InitAsDescriptorTable(1, &geometry_srv_range);

					D3D12_STATIC_SAMPLER_DESC linear_wrap_sampler = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
						D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
					root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 1, &linear_wrap_sampler);

					Microsoft::WRL::ComPtr<ID3DBlob> signature;
					Microsoft::WRL::ComPtr<ID3DBlob> error;
					D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
					if (error)
					{
						ADRIA_ASSERT(FALSE);
					}
					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::RayTracedReflections])));
				}

				{
					std::array<CD3DX12_ROOT_PARAMETER1, 8> root_parameters{};
					root_parameters[0].InitAsConstantBufferView(0);
					root_parameters[1].InitAsConstantBufferView(2);
					root_parameters[2].InitAsConstantBufferView(10);
					root_parameters[3].InitAsShaderResourceView(0);

					D3D12_DESCRIPTOR_RANGE1 srv_range{};
					srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					srv_range.NumDescriptors = 1;
					srv_range.BaseShaderRegister = 2;
					srv_range.RegisterSpace = 0;
					srv_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[4].InitAsDescriptorTable(1, &srv_range);

					D3D12_DESCRIPTOR_RANGE1 uav_range{};
					uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					uav_range.NumDescriptors = 2;
					uav_range.BaseShaderRegister = 0;
					uav_range.RegisterSpace = 0;
					uav_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[5].InitAsDescriptorTable(1, &uav_range);

					D3D12_DESCRIPTOR_RANGE1 unbounded_srv_range{};
					unbounded_srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					unbounded_srv_range.NumDescriptors = UINT_MAX;
					unbounded_srv_range.BaseShaderRegister = 0;
					unbounded_srv_range.RegisterSpace = 1;
					unbounded_srv_range.OffsetInDescriptorsFromTableStart = 0;
					unbounded_srv_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
					root_parameters[6].InitAsDescriptorTable(1, &unbounded_srv_range);

					D3D12_DESCRIPTOR_RANGE1 geometry_srv_range{};
					geometry_srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					geometry_srv_range.NumDescriptors = 3;
					geometry_srv_range.BaseShaderRegister = 0;
					geometry_srv_range.RegisterSpace = 2;
					geometry_srv_range.OffsetInDescriptorsFromTableStart = 0;
					root_parameters[7].InitAsDescriptorTable(1, &geometry_srv_range);

					D3D12_STATIC_SAMPLER_DESC linear_wrap_sampler = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
						D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

					CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
					root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 1, &linear_wrap_sampler);

					Microsoft::WRL::ComPtr<ID3DBlob> signature;
					Microsoft::WRL::ComPtr<ID3DBlob> error;
					D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
					if (error)
					{
						ADRIA_ASSERT(FALSE);
					}
					BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::PathTracing])));
				}
			}
		}
	}

	namespace RootSignatureCache
	{
		void Initialize(GraphicsDevice* gfx)
		{
			CreateAllRootSignatures(gfx->GetDevice());
		}
		void Destroy()
		{
			auto FreeContainer = []<typename T>(T & container)
			{
				container.clear();
				T empty;
				using std::swap;
				swap(container, empty);
			};
			FreeContainer(rs_map);
		}
		ID3D12RootSignature* Get(ERootSignature root_signature_id)
		{
			return rs_map[root_signature_id].Get();
		}
	}
}

