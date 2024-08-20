#include <array>
#include <memory>
#include "GfxCommon.h"
#include "GfxDevice.h"
#include "GfxTexture.h"
#include "GfxDescriptorAllocator.h"

namespace adria
{
	namespace gfxcommon
	{
		using enum GfxCommonTextureType;
		namespace
		{
			std::array<std::unique_ptr<GfxTexture>, (uint64)Count>	common_textures;
			std::unique_ptr<GfxDescriptorAllocator> common_views_heap;

			void CreateCommonTextures(GfxDevice* gfx)
			{
				GfxTextureDesc desc{};
				desc.width = 1;
				desc.height = 1;
				desc.format = GfxFormat::R8G8B8A8_UNORM;
				desc.bind_flags = GfxBindFlag::ShaderResource;
				desc.initial_state = GfxResourceState::PixelSRV;

				GfxTextureSubData init_data{};
				GfxTextureData data{};
				data.sub_data = &init_data;
				data.sub_count = 1;

				uint8 white[] = { 0xff, 0xff, 0xff, 0xff };
				init_data.data = white;
				init_data.row_pitch = sizeof(white);
				init_data.slice_pitch = 0;
				common_textures[(uint64)WhiteTexture2D] = gfx->CreateTexture(desc, data);
				
				uint8 black[] = { 0x00, 0x00, 0x00, 0xff };
				init_data.data = black;
				init_data.row_pitch = sizeof(black);
				common_textures[(uint64)BlackTexture2D] = gfx->CreateTexture(desc, data);

				GfxTextureDesc default_normal_desc{};
				default_normal_desc.width = 1;
				default_normal_desc.height = 1;
				default_normal_desc.format = GfxFormat::R8G8B8A8_UNORM;
				default_normal_desc.bind_flags = GfxBindFlag::ShaderResource;
				default_normal_desc.initial_state = GfxResourceState::PixelSRV;

				uint8 default_normal[] = { 0x7f, 0x7f, 0xff, 0xff };
				init_data.data = default_normal;
				init_data.row_pitch = sizeof(default_normal);
				init_data.slice_pitch = 0;
				common_textures[(uint64)DefaultNormal2D] = gfx->CreateTexture(default_normal_desc, data);

				GfxTextureDesc metallic_roughness_desc{};
				metallic_roughness_desc.width = 1;
				metallic_roughness_desc.height = 1;
				metallic_roughness_desc.format = GfxFormat::R8G8B8A8_UNORM;
				metallic_roughness_desc.bind_flags = GfxBindFlag::ShaderResource;
				metallic_roughness_desc.initial_state = GfxResourceState::PixelSRV;

				uint8 metallic_roughness[] = { 0xff, 0x7f, 0x00, 0xff };
				init_data.data = metallic_roughness;
				init_data.row_pitch = sizeof(metallic_roughness);
				init_data.slice_pitch = 0;
				common_textures[(uint64)MetallicRoughness2D] = gfx->CreateTexture(metallic_roughness_desc, data);
			}

			void CreateCommonViews(GfxDevice* gfx)
			{
				using enum GfxCommonViewType;

				ID3D12Device* device = gfx->GetDevice();

				GfxDescriptorAllocatorDesc desc{};
				desc.type = GfxDescriptorHeapType::CBV_SRV_UAV;
				desc.shader_visible = false;
				desc.descriptor_count = (uint64)Count;

				common_views_heap = std::make_unique<GfxDescriptorAllocator>(gfx, desc);
				D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
				null_srv_desc.Texture2D.MostDetailedMip = 0;
				null_srv_desc.Texture2D.MipLevels = -1;
				null_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

				null_srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

				device->CreateShaderResourceView(nullptr, &null_srv_desc, common_views_heap->GetHandle((uint64)NullTexture2D_SRV));
				null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				device->CreateShaderResourceView(nullptr, &null_srv_desc, common_views_heap->GetHandle((uint64)NullTextureCube_SRV));
				null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				device->CreateShaderResourceView(nullptr, &null_srv_desc, common_views_heap->GetHandle((uint64)NullTexture2DArray_SRV));

				D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc{};
				null_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				null_uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				device->CreateUnorderedAccessView(nullptr, nullptr, &null_uav_desc, common_views_heap->GetHandle((uint64)NullTexture2D_UAV));

				GfxDescriptor white_srv = gfx->CreateTextureSRV(common_textures[(uint64)WhiteTexture2D].get());
				GfxDescriptor black_srv = gfx->CreateTextureSRV(common_textures[(uint64)BlackTexture2D].get());
				GfxDescriptor default_normal_srv = gfx->CreateTextureSRV(common_textures[(uint64)DefaultNormal2D].get());
				GfxDescriptor metallic_roughness_srv = gfx->CreateTextureSRV(common_textures[(uint64)MetallicRoughness2D].get());

				gfx->CopyDescriptors(1, common_views_heap->GetHandle((uint64)WhiteTexture2D_SRV), white_srv);
				gfx->CopyDescriptors(1, common_views_heap->GetHandle((uint64)BlackTexture2D_SRV), black_srv);
				gfx->CopyDescriptors(1, common_views_heap->GetHandle((uint64)DefaultNormal2D_SRV), default_normal_srv);
				gfx->CopyDescriptors(1, common_views_heap->GetHandle((uint64)MetallicRoughness2D_SRV), metallic_roughness_srv);
			}
		}

		void Initialize(GfxDevice* gfx)
		{
			CreateCommonTextures(gfx);
			CreateCommonViews(gfx);
		}

		void Destroy()
		{
			common_views_heap.reset();
			for (auto& texture : common_textures) texture.reset();
		}

		GfxTexture* GetCommonTexture(GfxCommonTextureType type)
		{
			return common_textures[(uint64)type].get();
		}

		GfxDescriptor GetCommonView(GfxCommonViewType type)
		{
			return common_views_heap->GetHandle((uint64)type);
		}

	}
}

