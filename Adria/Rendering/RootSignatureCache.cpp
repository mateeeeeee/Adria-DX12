#include "RootSignatureCache.h"
#include "ShaderCache.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Graphics/Shader.h"
#include "../Utilities/HashMap.h"

using namespace Microsoft::WRL;

namespace adria
{
	namespace
	{
		HashMap<ERootSignature, ComPtr<ID3D12RootSignature>> rs_map;
		inline Shader const& GetShader(EShaderId shader)
		{
			return ShaderCache::GetShader(shader);
		}

		void CreateRootSignaturesFromHLSL(ID3D12Device* device)
		{
			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Skybox).GetPointer(), GetShader(PS_Skybox).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Skybox].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_HosekWilkieSky).GetPointer(), GetShader(PS_HosekWilkieSky).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Sky].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Taa).GetPointer(), GetShader(PS_Taa).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::TAA].GetAddressOf())));

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

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Add).GetPointer(), GetShader(PS_Add).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Add].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_TiledLighting).GetPointer(), GetShader(CS_TiledLighting).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::TiledLighting].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ClusterBuilding).GetPointer(), GetShader(CS_ClusterBuilding).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::ClusterBuilding].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(CS_ClusterCulling).GetPointer(), GetShader(CS_ClusterCulling).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::ClusterCulling].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Copy).GetPointer(), GetShader(PS_Copy).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Copy].GetAddressOf())));

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

			BREAK_IF_FAILED(device->CreateRootSignature(0, GetShader(PS_Decals).GetPointer(), GetShader(PS_Decals).GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Decals].GetAddressOf())));

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

			/*ID3D12VersionedRootSignatureDeserializer* drs = nullptr;
			D3D12CreateVersionedRootSignatureDeserializer(GetShader(PS_Add).GetPointer(), shader_map[PS_Add].GetLength(), IID_PPV_ARGS(&drs));
			D3D12_VERSIONED_ROOT_SIGNATURE_DESC const* desc = drs->GetUnconvertedRootSignatureDesc();*/
		}
		void CreateRootSignaturesFromCpp(ID3D12Device* device)
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
				feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

			{
				CD3DX12_ROOT_PARAMETER1 root_parameters[3] = {};
				root_parameters[0].InitAsConstantBufferView(0);
				root_parameters[1].InitAsConstants(8, 1);
				root_parameters[2].InitAsConstantBufferView(2);

				D3D12_ROOT_SIGNATURE_FLAGS flags =
					D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
					D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

				CD3DX12_STATIC_SAMPLER_DESC static_samplers[8] = {};
				static_samplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP,   D3D12_TEXTURE_ADDRESS_MODE_WRAP,   D3D12_TEXTURE_ADDRESS_MODE_WRAP);
				static_samplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
				static_samplers[2].Init(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);

				static_samplers[3].Init(3, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
				static_samplers[4].Init(4, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
				static_samplers[5].Init(5, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);

				static_samplers[6].Init(6, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 16u, D3D12_COMPARISON_FUNC_GREATER);
				static_samplers[7].Init(7, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0.0f, 16u, D3D12_COMPARISON_FUNC_GREATER);
				
				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
				desc.Init_1_1(ARRAYSIZE(root_parameters), root_parameters, ARRAYSIZE(static_samplers), static_samplers, flags);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
				BREAK_IF_FAILED(hr);
				hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::Common]));
				BREAK_IF_FAILED(hr);
			}

			{
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

		void CreateAllRootSignatures(ID3D12Device* device)
		{
			CreateRootSignaturesFromHLSL(device);
			CreateRootSignaturesFromCpp(device);
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
			auto FreeContainer = []<typename T>(T& container)
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

