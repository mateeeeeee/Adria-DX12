#include "DecalsPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../tecs/registry.h"

using namespace DirectX;

namespace adria
{

	DecalsPass::DecalsPass(tecs::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h)
	 : reg{ reg }, texture_manager{ texture_manager }, width{ w }, height{ h }
	{}

	void DecalsPass::AddPass(RenderGraph& rendergraph)
	{
		if (reg.size<Decal>() == 0) return;
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct DecalsPassData
		{
			RGTextureReadOnlyId depth_srv;
		};
		rendergraph.AddPass<DecalsPassData>("Decals Pass",
			[=](DecalsPassData& data, RenderGraphBuilder& builder)
			{
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferNormal), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](DecalsPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto upload_buffer = gfx->GetDynamicAllocator();

				struct DecalCBuffer
				{
					int32 decal_type;
				};
				DecalCBuffer decal_cbuf_data{};
				ObjectCBuffer object_cbuf_data{};

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Decals));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				auto decal_view = reg.view<Decal>();

				auto decal_pass_lambda = [&](bool modify_normals)
				{
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(modify_normals ? EPipelineStateObject::Decals_ModifyNormals : EPipelineStateObject::Decals));
					for (auto e : decal_view)
					{
						Decal decal = decal_view.get(e);
						if (decal.modify_gbuffer_normals != modify_normals) continue;

						object_cbuf_data.model = decal.decal_model_matrix;
						object_cbuf_data.inverse_transposed_model = XMMatrixTranspose(XMMatrixInverse(nullptr, object_cbuf_data.model));
						DynamicAllocation object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						object_allocation.Update(object_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
						cmd_list->SetGraphicsRoot32BitConstant(2, static_cast<UINT>(decal.decal_type), 0);

						std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles{};
						std::vector<uint32> src_range_sizes{};

						texture_handles.push_back(texture_manager.CpuDescriptorHandle(decal.albedo_decal_texture));
						texture_handles.push_back(texture_manager.CpuDescriptorHandle(decal.normal_decal_texture));
						texture_handles.push_back(context.GetReadOnlyTexture(data.depth_srv));
						src_range_sizes.assign(texture_handles.size(), 1u);

						OffsetType descriptor_index = descriptor_allocator->AllocateRange(texture_handles.size());
						auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
						uint32 dst_range_sizes[] = { (uint32)texture_handles.size() };
						device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, (uint32)texture_handles.size(), texture_handles.data(), src_range_sizes.data(),
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						cmd_list->SetGraphicsRootDescriptorTable(3, dst_descriptor);

						cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
						BindVertexBuffer(cmd_list, cube_vb.get());
						BindIndexBuffer(cmd_list, cube_ib.get());
						cmd_list->DrawIndexedInstanced(cube_ib->GetCount(), 1, 0, 0, 0);
					}
				};
				decal_pass_lambda(false);
				decal_pass_lambda(true);
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void DecalsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void DecalsPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		CreateCubeBuffers(gfx);
	}

	void DecalsPass::CreateCubeBuffers(GraphicsDevice* gfx)
	{
		SimpleVertex const cube_vertices[8] =
		{
			XMFLOAT3{ -0.5f, -0.5f,  0.5f },
			XMFLOAT3{  0.5f, -0.5f,  0.5f },
			XMFLOAT3{  0.5f,  0.5f,  0.5f },
			XMFLOAT3{ -0.5f,  0.5f,  0.5f },
			XMFLOAT3{ -0.5f, -0.5f, -0.5f },
			XMFLOAT3{  0.5f, -0.5f, -0.5f },
			XMFLOAT3{  0.5f,  0.5f, -0.5f },
			XMFLOAT3{ -0.5f,  0.5f, -0.5f }
		};

		uint16_t const cube_indices[36] =
		{
			// front
			0, 1, 2,
			2, 3, 0,
			// right
			1, 5, 6,
			6, 2, 1,
			// back
			7, 6, 5,
			5, 4, 7,
			// left
			4, 0, 3,
			3, 7, 4,
			// bottom
			4, 5, 1,
			1, 0, 4,
			// top
			3, 2, 6,
			6, 7, 3
		};

		BufferDesc vb_desc{};
		vb_desc.bind_flags = EBindFlag::None;
		vb_desc.size = sizeof(cube_vertices);
		vb_desc.stride = sizeof(SimpleVertex);
		cube_vb = std::make_unique<Buffer>(gfx, vb_desc, cube_vertices);

		BufferDesc ib_desc{};
		ib_desc.bind_flags = EBindFlag::None;
		ib_desc.format = DXGI_FORMAT_R16_UINT;
		ib_desc.stride = sizeof(uint16);
		ib_desc.size = sizeof(cube_indices);
		cube_ib = std::make_unique<Buffer>(gfx, ib_desc, cube_indices);
	}

}

