#include <array>
#include <memory>
#include "GraphicsCommon.h"
#include "Texture.h"
#include "DescriptorHeap.h"

namespace adria
{
	namespace gfxcommon
	{
		namespace
		{
			std::array<std::unique_ptr<Texture>, (size_t)ECommonTextureType::Count>	common_textures;
			std::unique_ptr<DescriptorHeap> common_views_heap;

			void CreateCommonTextures(GraphicsDevice* gfx)
			{
				TextureDesc desc{};
				desc.width = 1;
				desc.height = 1;
				desc.format = EFormat::R32_FLOAT;
				desc.bind_flags = EBindFlag::ShaderResource;
				desc.initial_state = EResourceState::PixelShaderResource;
				desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				TextureInitialData init_data{};
				float v = 1.0f;
				init_data.pData = &v;
				init_data.RowPitch = sizeof(float);
				init_data.SlicePitch = 0;
				common_textures[(size_t)ECommonTextureType::WhiteTexture2D] = std::make_unique<Texture>(gfx, desc, &init_data);
				common_textures[(size_t)ECommonTextureType::WhiteTexture2D]->CreateSRV();

				v = 0.0f;
				common_textures[(size_t)ECommonTextureType::BlackTexture2D] = std::make_unique<Texture>(gfx, desc, &init_data);
				common_textures[(size_t)ECommonTextureType::BlackTexture2D]->CreateSRV();
			}

			void CreateCommonViews(GraphicsDevice* gfx)
			{
				using enum ECommonViewType;

				ID3D12Device* device = gfx->GetDevice();
				common_views_heap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, (size_t)Count);
				D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
				null_srv_desc.Texture2D.MostDetailedMip = 0;
				null_srv_desc.Texture2D.MipLevels = -1;
				null_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

				null_srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

				device->CreateShaderResourceView(nullptr, &null_srv_desc, common_views_heap->GetHandle((size_t)NullTexture2D_SRV));
				null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				device->CreateShaderResourceView(nullptr, &null_srv_desc, common_views_heap->GetHandle((size_t)NullTextureCube_SRV));
				null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				device->CreateShaderResourceView(nullptr, &null_srv_desc, common_views_heap->GetHandle((size_t)NullTexture2DArray_SRV));

				D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc{};
				null_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				null_uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				device->CreateUnorderedAccessView(nullptr, nullptr, &null_uav_desc, common_views_heap->GetHandle((size_t)NullTexture2D_UAV));
			}
		}

		void Initialize(GraphicsDevice* gfx)
		{
			CreateCommonTextures(gfx);
			CreateCommonViews(gfx);
		}

		void Destroy()
		{
			common_views_heap.reset();
			for (auto& texture  : common_textures) texture.reset();
		}

		Texture* GetCommonTexture(ECommonTextureType type)
		{
			return common_textures[(size_t)type].get();
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetCommonView(ECommonViewType type)
		{
			return common_views_heap->GetHandle((size_t)type);
		}

	}
}

