#include "SkyPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "Enums.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	SkyPass::SkyPass(entt::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h)
		: reg(reg), texture_manager(texture_manager), width(w), height(h)
	{}

	void SkyPass::AddPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rg.AddPass<void>("Sky Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.ReadDepthStencil(RG_RES_NAME(DepthStencil), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
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
					cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Skybox));
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Skybox));

					cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

					auto skybox_view = reg.view<Skybox>();
					for (auto e : skybox_view)
					{
						auto const& skybox = skybox_view.get<Skybox>(e);
						if (!skybox.active) continue;

						OffsetType descriptor_index = descriptor_allocator->Allocate();
						ADRIA_ASSERT(skybox.cubemap_texture != INVALID_TEXTURE_HANDLE);
						D3D12_CPU_DESCRIPTOR_HANDLE texture_handle = texture_manager.GetSRV(skybox.cubemap_texture);

						auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
						device->CopyDescriptorsSimple(1, dst_descriptor, texture_handle,
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

						cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);
					}
					break;
				}
				case ESkyType::UniformColor:
				{
					cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Sky));
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::UniformColorSky));
					cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
					cmd_list->SetGraphicsRootConstantBufferView(2, global_data.weather_cbuffer_address);
					break;
				}
				case ESkyType::HosekWilkie:
				{
					cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Sky));
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::HosekWilkieSky));
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
		
		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Sky", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					static int current_sky_type = 0;
					const char* sky_types[] = { "Skybox", "Uniform Color", "Hosek-Wilkie" };
					const char* combo_label = sky_types[current_sky_type];
					if (ImGui::BeginCombo("Sky Type", combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(sky_types); n++)
						{
							const bool is_selected = (current_sky_type == n);
							if (ImGui::Selectable(sky_types[n], is_selected)) current_sky_type = n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (current_sky_type == 0) sky_type = ESkyType::Skybox;
					else if (current_sky_type == 1)
					{
						sky_type = ESkyType::UniformColor;
						static char const* const sky_colors[] = { "Deep Sky Blue", "Sky Blue", "Light Sky Blue" };
						static int current_sky_color = 0;
						ImGui::ListBox("Tone Map Operator", &current_sky_color, sky_colors, IM_ARRAYSIZE(sky_colors));

						switch (current_sky_color)
						{
						case 0:
						{
							static float32 deep_sky_blue[3] = { 0.0f, 0.75f, 1.0f };
							memcpy(sky_color, deep_sky_blue, sizeof(deep_sky_blue));
							break;
						}
						case 1:
						{
							static float32 sky_blue[3] = { 0.53f, 0.81f, 0.92f };
							memcpy(sky_color, sky_blue, sizeof(sky_blue));
							break;
						}
						case 2:
						{
							static float32 light_sky_blue[3] = { 0.53f, 0.81f, 0.98f };
							memcpy(sky_color, light_sky_blue, sizeof(light_sky_blue));
							break;
						}
						default:
							ADRIA_ASSERT(false);
						}
					}
					else if (current_sky_type == 2)
					{
						sky_type = ESkyType::HosekWilkie;
						ImGui::SliderFloat("Turbidity", &turbidity, 2.0f, 30.0f);
						ImGui::SliderFloat("Ground Albedo", &ground_albedo, 0.0f, 1.0f);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
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
		vb_desc.bind_flags = EBindFlag::None;
		vb_desc.size = sizeof(cube_vertices);
		vb_desc.stride = sizeof(SimpleVertex);
		cube_vb = std::make_unique<Buffer>(gfx, vb_desc, cube_vertices);

		BufferDesc ib_desc{};
		ib_desc.bind_flags = EBindFlag::None;
		ib_desc.format = EFormat::R16_UINT;
		ib_desc.stride = sizeof(uint16);
		ib_desc.size = sizeof(cube_indices);
		cube_ib = std::make_unique<Buffer>(gfx, ib_desc, cube_indices);
	}

}