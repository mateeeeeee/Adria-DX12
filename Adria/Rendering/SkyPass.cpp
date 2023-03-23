#include "SkyPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "Enums.h"
#include "BlackboardData.h"
#include "PSOCache.h"

#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../Graphics/GfxLinearDynamicAllocator.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	SkyPass::SkyPass(entt::registry& reg, uint32 w, uint32 h)
		: reg(reg), width(w), height(h), sky_type(SkyType::Skybox)
	{}

	void SkyPass::AddPass(RenderGraph& rg, DirectX::XMFLOAT3 const& dir)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		rg.AddPass<void>("Sky Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.ReadDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				struct SkyConstants
				{
					XMMATRIX model_matrix;
				} constants =
				{
					.model_matrix = XMMatrixTranslationFromVector(global_data.camera_position)
				};

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);

				switch (sky_type)
				{
				case SkyType::Skybox:
				{
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Skybox));
					auto skybox_view = reg.view<Skybox>();
					for (auto e : skybox_view)
					{
						auto const& skybox = skybox_view.get<Skybox>(e);
						if (!skybox.active) continue;

						ADRIA_ASSERT(skybox.cubemap_texture != INVALID_TEXTURE_HANDLE);
						GfxDescriptor texture_handle = TextureManager::Get().GetSRV(skybox.cubemap_texture);
						cmd_list->SetRootConstant(1, (uint32)skybox.cubemap_texture, 0);
					}
					break;
				}
				case SkyType::UniformColor:
				{
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::UniformColorSky));
					struct UniformColorSkyConstants
					{
						XMFLOAT3 sky_color;
					} constants =
					{
						.sky_color = XMFLOAT3(sky_color)
					};
					cmd_list->SetRootConstants(1, constants);
					break;
				}
				case SkyType::HosekWilkie:
				{
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::HosekWilkieSky));
					SkyParameters parameters = CalculateSkyParameters(turbidity, ground_albedo, dir);
					DECLSPEC_ALIGN(16) struct HosekWilkieConstants
					{
						DECLSPEC_ALIGN(16) XMFLOAT3 A;
						DECLSPEC_ALIGN(16) XMFLOAT3 B;
						DECLSPEC_ALIGN(16) XMFLOAT3 C;
						DECLSPEC_ALIGN(16) XMFLOAT3 D;
						DECLSPEC_ALIGN(16) XMFLOAT3 E;
						DECLSPEC_ALIGN(16) XMFLOAT3 F;
						DECLSPEC_ALIGN(16) XMFLOAT3 G;
						DECLSPEC_ALIGN(16) XMFLOAT3 H;
						DECLSPEC_ALIGN(16) XMFLOAT3 I;
						DECLSPEC_ALIGN(16) XMFLOAT3 Z;
					} constants =
					{
						.A = parameters[ESkyParam_A],
						.B = parameters[ESkyParam_B],
						.C = parameters[ESkyParam_C],
						.D = parameters[ESkyParam_D],
						.E = parameters[ESkyParam_E],
						.F = parameters[ESkyParam_F],
						.G = parameters[ESkyParam_G],
						.H = parameters[ESkyParam_H],
						.I = parameters[ESkyParam_I],
						.Z = parameters[ESkyParam_Z],
					};
					cmd_list->SetRootCBV(3, constants);
					break;
				}
				default:
					ADRIA_ASSERT(false);
				}

				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
				BindVertexBuffer(cmd_list->GetNative(), cube_vb.get());
				BindIndexBuffer(cmd_list->GetNative(), cube_ib.get());
				cmd_list->DrawIndexed(cube_ib->GetCount());
			}, RGPassType::Graphics, RGPassFlags::None);

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

					if (current_sky_type == 0) sky_type = SkyType::Skybox;
					else if (current_sky_type == 1)
					{
						sky_type = SkyType::UniformColor;
						static char const* const sky_colors[] = { "Deep Sky Blue", "Sky Blue", "Light Sky Blue" };
						static int current_sky_color = 0;
						ImGui::ListBox("Tone Map Operator", &current_sky_color, sky_colors, IM_ARRAYSIZE(sky_colors));

						switch (current_sky_color)
						{
						case 0:
						{
							static float deep_sky_blue[3] = { 0.0f, 0.75f, 1.0f };
							memcpy(sky_color, deep_sky_blue, sizeof(deep_sky_blue));
							break;
						}
						case 1:
						{
							static float sky_blue[3] = { 0.53f, 0.81f, 0.92f };
							memcpy(sky_color, sky_blue, sizeof(sky_blue));
							break;
						}
						case 2:
						{
							static float light_sky_blue[3] = { 0.53f, 0.81f, 0.98f };
							memcpy(sky_color, light_sky_blue, sizeof(light_sky_blue));
							break;
						}
						default:
							ADRIA_ASSERT(false);
						}
					}
					else if (current_sky_type == 2)
					{
						sky_type = SkyType::HosekWilkie;
						ImGui::SliderFloat("Turbidity", &turbidity, 2.0f, 30.0f);
						ImGui::SliderFloat("Ground Albedo", &ground_albedo, 0.0f, 1.0f);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

	void SkyPass::OnSceneInitialized(GfxDevice* gfx)
	{
		CreateCubeBuffers(gfx);
	}

	void SkyPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void SkyPass::CreateCubeBuffers(GfxDevice* gfx)
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

		GfxBufferDesc vb_desc{};
		vb_desc.bind_flags = GfxBindFlag::None;
		vb_desc.size = sizeof(cube_vertices);
		vb_desc.stride = sizeof(SimpleVertex);
		cube_vb = std::make_unique<GfxBuffer>(gfx, vb_desc, cube_vertices);

		GfxBufferDesc ib_desc{};
		ib_desc.bind_flags = GfxBindFlag::None;
		ib_desc.format = GfxFormat::R16_UINT;
		ib_desc.stride = sizeof(uint16);
		ib_desc.size = sizeof(cube_indices);
		cube_ib = std::make_unique<GfxBuffer>(gfx, ib_desc, cube_indices);
	}

}