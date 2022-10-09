#include "VelocityBufferPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

namespace adria
{
	VelocityBufferPass::VelocityBufferPass(uint32 w, uint32 h)
		: width(w), height(h)
	{}

	void VelocityBufferPass::AddPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct VelocityBufferPassData
		{
			RGTextureReadOnlyId depth_srv;
		};
		rg.AddPass<VelocityBufferPassData>("Velocity Buffer Pass",
			[=](VelocityBufferPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc velocity_buffer_desc{};
				velocity_buffer_desc.width = width;
				velocity_buffer_desc.height = height;
				velocity_buffer_desc.format = EFormat::R16G16_FLOAT;

				builder.SetViewport(width, height);
				builder.DeclareTexture(RG_RES_NAME(VelocityBuffer), velocity_buffer_desc);
				builder.WriteRenderTarget(RG_RES_NAME(VelocityBuffer), ERGLoadStoreAccessOp::Discard_Preserve);
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
			},
			[=](VelocityBufferPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::VelocityBuffer));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::VelocityBuffer));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(1);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
					context.GetReadOnlyTexture(data.depth_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);

		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("Velocity Buffer", 0))
				{
					ImGui::SliderFloat("Velocity Buffer Scale", &params.velocity_buffer_scale, 32.0f, 128.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			});
	}

	void VelocityBufferPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}
}

