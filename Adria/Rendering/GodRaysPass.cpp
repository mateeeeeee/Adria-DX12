#include <DirectXMath.h>
#include "GodRaysPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../Logging/Logger.h"
#include "../Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{

	GodRaysPass::GodRaysPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName GodRaysPass::AddPass(RenderGraph& rg, Light const& light)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct GodRaysPassData
		{
			RGTextureReadOnlyId sun_output;
		};

		rg.AddPass<GodRaysPassData>("God Rays Pass",
			[=](GodRaysPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc god_rays_desc{};
				god_rays_desc.format = EFormat::R16G16B16A16_FLOAT;
				god_rays_desc.width = width;
				god_rays_desc.height = height;
				god_rays_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_RES_NAME(GodRaysOutput), god_rays_desc);
				builder.WriteRenderTarget(RG_RES_NAME(GodRaysOutput), ERGLoadStoreAccessOp::Clear_Preserve);
				data.sun_output = builder.ReadTexture(RG_RES_NAME(SunOutput), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](GodRaysPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				if (light.type != ELightType::Directional)
				{
					ADRIA_LOG(WARNING, "Using God Rays on a Non-Directional Light Source");
					return;
				}

				LightCBuffer light_cbuf_data{};
				{
					light_cbuf_data.godrays_decay = light.godrays_decay;
					light_cbuf_data.godrays_density = light.godrays_density;
					light_cbuf_data.godrays_exposure = light.godrays_exposure;
					light_cbuf_data.godrays_weight = light.godrays_weight;

					auto camera_position = global_data.camera_position;
					XMVECTOR light_position = light.type == ELightType::Directional ?
						XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position)))
						: light.position;

					DirectX::XMVECTOR light_pos_h = XMVector4Transform(light_position, global_data.camera_viewproj);
					DirectX::XMFLOAT4 light_pos{};
					DirectX::XMStoreFloat4(&light_pos, light_pos_h);
					light_cbuf_data.ss_position = XMVectorSet(0.5f * light_pos.x / light_pos.w + 0.5f, -0.5f * light_pos.y / light_pos.w + 0.5f, light_pos.z / light_pos.w, 1.0f);

					static const float32 f_max_sun_dist = 1.3f;
					float32 f_max_dist = (std::max)(abs(XMVectorGetX(light_cbuf_data.ss_position)), abs(XMVectorGetY(light_cbuf_data.ss_position)));
					if (f_max_dist >= 1.0f)
						light_cbuf_data.color = XMVector3Transform(light.color, XMMatrixScaling((f_max_sun_dist - f_max_dist), (f_max_sun_dist - f_max_dist), (f_max_sun_dist - f_max_dist)));
					else light_cbuf_data.color = light.color;
				}

				DynamicAllocation light_allocation = dynamic_allocator->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				light_allocation.Update(light_cbuf_data);

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::GodRays));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::GodRays));

				cmd_list->SetGraphicsRootConstantBufferView(0, light_allocation.gpu_address);

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = context.GetReadOnlyTexture(data.sun_output);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void GodRaysPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}
