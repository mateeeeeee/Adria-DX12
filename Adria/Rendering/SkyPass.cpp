#include "SkyPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "Enums.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "../tecs/registry.h"

using namespace DirectX;

namespace adria
{

	SkyPass::SkyPass(tecs::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h)
		: reg(reg), texture_manager(texture_manager), width(w), height(h)
	{}

	void SkyPass::AddPass(RenderGraph& rg, ESkyType sky_type)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rg.AddPass<void>("Sky Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.ReadDepthStencil(RG_RES_NAME(DepthStencil), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=, this](RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto upload_buffer = gfx->GetDynamicAllocator();

				ObjectCBuffer object_cbuf_data{};
				object_cbuf_data.model = DirectX::XMMatrixTranslationFromVector(global_data.camera_position);
				DynamicAllocation object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				object_allocation.Update(object_cbuf_data);

				switch (sky_type)
				{
				case ESkyType::Skybox:
				{
					cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Skybox));
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Skybox));

					cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

					auto skybox_view = reg.view<Skybox>();
					for (auto e : skybox_view)
					{
						auto const& skybox = skybox_view.get(e);
						if (!skybox.active) continue;

						OffsetType descriptor_index = descriptor_allocator->Allocate();
						ADRIA_ASSERT(skybox.cubemap_texture != INVALID_TEXTURE_HANDLE);
						D3D12_CPU_DESCRIPTOR_HANDLE texture_handle = texture_manager.CpuDescriptorHandle(skybox.cubemap_texture);

						auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
						device->CopyDescriptorsSimple(1, dst_descriptor, texture_handle,
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

						cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);
					}
					break;
				}
				case ESkyType::UniformColor:
				{
					cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Sky));
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::UniformColorSky));
					cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
					cmd_list->SetGraphicsRootConstantBufferView(2, global_data.weather_cbuffer_address);
					break;
				}
				case ESkyType::HosekWilkie:
				{
					cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Sky));
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::HosekWilkieSky));
					cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
					cmd_list->SetGraphicsRootConstantBufferView(2, global_data.weather_cbuffer_address);
					break;
				}
				default:
					ADRIA_ASSERT(false);
				}

				cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				BindVertexBuffer(cmd_list, cube_vb.get());
				BindIndexBuffer(cmd_list, cube_ib.get());
				cmd_list->DrawIndexedInstanced(cube_ib->GetCount(), 1, 0, 0, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void SkyPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		CreateCubeBuffers(gfx);
	}

	void SkyPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void SkyPass::CreateCubeBuffers(GraphicsDevice* gfx)
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
		vb_desc.bind_flags = EBindFlag::VertexBuffer;
		vb_desc.size = sizeof(cube_vertices);
		vb_desc.stride = sizeof(SimpleVertex);
		cube_vb = std::make_unique<Buffer>(gfx, vb_desc, cube_vertices);

		BufferDesc ib_desc{};
		ib_desc.bind_flags = EBindFlag::IndexBuffer;
		ib_desc.format = DXGI_FORMAT_R16_UINT;
		ib_desc.stride = sizeof(uint16);
		ib_desc.size = sizeof(cube_indices);
		cube_ib = std::make_unique<Buffer>(gfx, ib_desc, cube_indices);
	}

}