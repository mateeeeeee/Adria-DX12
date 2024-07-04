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
		namespace
		{
			std::array<std::unique_ptr<GfxTexture>, (uint64)GfxCommonTextureType::Count>	common_textures;
			std::unique_ptr<GfxDescriptorAllocator> common_views_heap;

			void CreateCommonTextures(GfxDevice* gfx)
			{
				GfxTextureDesc desc{};
				desc.width = 1;
				desc.height = 1;
				desc.format = GfxFormat::R32_FLOAT;
				desc.bind_flags = GfxBindFlag::ShaderResource;
				desc.initial_state = GfxResourceState::PixelSRV;
				desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				GfxTextureInitialData init_data{};
				float v = 1.0f;
				init_data.data = &v;
				init_data.row_pitch = sizeof(float);
				init_data.slice_pitch = 0;
				common_textures[(uint64)GfxCommonTextureType::WhiteTexture2D] = gfx->CreateTexture(desc, &init_data);
				v = 0.0f;
				common_textures[(uint64)GfxCommonTextureType::BlackTexture2D] = gfx->CreateTexture(desc, &init_data);
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

				GfxDescriptor white_srv = gfx->CreateTextureSRV(common_textures[(uint64)GfxCommonTextureType::WhiteTexture2D].get());
				GfxDescriptor black_srv = gfx->CreateTextureSRV(common_textures[(uint64)GfxCommonTextureType::BlackTexture2D].get());

				gfx->CopyDescriptors(1, common_views_heap->GetHandle((uint64)WhiteTexture2D_SRV), white_srv);
				gfx->CopyDescriptors(1, common_views_heap->GetHandle((uint64)BlackTexture2D_SRV), black_srv);
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

