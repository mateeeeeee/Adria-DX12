#include "LensFlarePass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../Logging/Logger.h"

using namespace DirectX;

namespace adria
{

	LensFlarePass::LensFlarePass(TextureManager& texture_manager, uint32 w, uint32 h)
		: texture_manager{ texture_manager}, width{ w }, height{ h }
	{}

	void LensFlarePass::AddPass(RenderGraph& rg, Light const& light)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct LensFlarePassData
		{
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<LensFlarePassData>("LensFlare Pass",
			[=](LensFlarePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(PostprocessMain), ERGLoadStoreAccessOp::Preserve_Preserve);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](LensFlarePassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				if (light.type != ELightType::Directional)
				{
					ADRIA_LOG(WARNING, "Using Lens Flare on a Non-Directional Light Source");
					return;
				}

				std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> lens_flare_descriptors{};
				for (size_t i = 0; i < lens_flare_textures.size(); ++i)
					lens_flare_descriptors.push_back(texture_manager.GetSRV(lens_flare_textures[i]));

				lens_flare_descriptors.push_back(context.GetReadOnlyTexture(data.depth));

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::LensFlare));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::LensFlare));
				DynamicAllocation light_allocation = dynamic_allocator->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				LightCBuffer light_cbuf_data{};
				{
					auto camera_position = global_data.camera_position;
					XMVECTOR light_position = light.type == ELightType::Directional ?
						XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position)))
						: light.position;
					DirectX::XMVECTOR light_pos_h = XMVector4Transform(light_position, global_data.camera_viewproj);
					DirectX::XMFLOAT4 light_pos{};
					DirectX::XMStoreFloat4(&light_pos, light_pos_h);
					light_cbuf_data.ss_position = XMVectorSet(0.5f * light_pos.x / light_pos.w + 0.5f, -0.5f * light_pos.y / light_pos.w + 0.5f, light_pos.z / light_pos.w, 1.0f);
				}
				light_allocation.Update(light_cbuf_data);

				cmd_list->SetGraphicsRootConstantBufferView(0, light_allocation.gpu_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(8);
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1, 1, 1, 1, 1, 1, 1 };
				uint32 dst_range_sizes[] = { 8 };
				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_range_sizes), lens_flare_descriptors.data(), src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
				cmd_list->DrawInstanced(7, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void LensFlarePass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void LensFlarePass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		lens_flare_textures.push_back(texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare0.jpg"));
		lens_flare_textures.push_back(texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare1.jpg"));
		lens_flare_textures.push_back(texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare2.jpg"));
		lens_flare_textures.push_back(texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare3.jpg"));
		lens_flare_textures.push_back(texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare4.jpg"));
		lens_flare_textures.push_back(texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare5.jpg"));
		lens_flare_textures.push_back(texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare6.jpg"));
	}

}

