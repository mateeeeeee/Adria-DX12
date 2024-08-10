#include "SkyPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "TextureManager.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxReflection.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{
	static constexpr uint32 SKYCUBE_SIZE = 128;

	SkyPass::SkyPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h)
		: reg(reg), gfx(gfx), width(w), height(h), sky_type(SkyType::HosekWilkie)
	{
		CreatePSOs();
	}

	void SkyPass::AddComputeSkyPass(RenderGraph& rg, Vector3 const& dir)
	{
		if (sky_type == SkyType::Skybox)
		{
			auto skybox_view = reg.view<Skybox>();
			for (auto e : skybox_view)
			{
				auto const& [skybox] = skybox_view.get(e);
				if (skybox.active)
				{
					GfxTexture* skybox_texture = g_TextureManager.GetTexture(skybox.cubemap_texture);
					rg.ImportTexture(RG_NAME(Sky), skybox_texture);
					break;
				}
			}
			return;
		}

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct ComputeSkyPassData
		{
			RGTextureReadWriteId sky_uav;
		};

		rg.ImportTexture(RG_NAME(Sky), sky_texture.get());
		rg.AddPass<ComputeSkyPassData>("Compute Sky Pass",
			[=](ComputeSkyPassData& data, RenderGraphBuilder& builder)
			{
				data.sky_uav = builder.WriteTexture(RG_NAME(Sky));
			},
			[&](ComputeSkyPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);

				GfxDescriptor sky = gfx->AllocateDescriptorsGPU(1);
				gfx->CopyDescriptors(1, sky, context.GetReadWriteTexture(data.sky_uav));
				cmd_list->SetRootConstant(1, sky.GetIndex());

				switch (sky_type)
				{
				case SkyType::MinimalAtmosphere:
				{
					cmd_list->SetPipelineState(minimal_atmosphere_pso.get());
					break;
				}
				case SkyType::HosekWilkie:
				{
					cmd_list->SetPipelineState(hosek_wilkie_pso.get());
					SkyParameters parameters = CalculateSkyParameters(turbidity, ground_albedo, dir);
					struct HosekWilkieConstants
					{
						DECLSPEC_ALIGN(16) Vector3 A;
						DECLSPEC_ALIGN(16) Vector3 B;
						DECLSPEC_ALIGN(16) Vector3 C;
						DECLSPEC_ALIGN(16) Vector3 D;
						DECLSPEC_ALIGN(16) Vector3 E;
						DECLSPEC_ALIGN(16) Vector3 F;
						DECLSPEC_ALIGN(16) Vector3 G;
						DECLSPEC_ALIGN(16) Vector3 H;
						DECLSPEC_ALIGN(16) Vector3 I;
						DECLSPEC_ALIGN(16) Vector3 Z;
					} constants =
					{
							.A = parameters[SkyParam_A],
							.B = parameters[SkyParam_B],
							.C = parameters[SkyParam_C],
							.D = parameters[SkyParam_D],
							.E = parameters[SkyParam_E],
							.F = parameters[SkyParam_F],
							.G = parameters[SkyParam_G],
							.H = parameters[SkyParam_H],
							.I = parameters[SkyParam_I],
							.Z = parameters[SkyParam_Z],
					};
					cmd_list->SetRootCBV(3, constants);
					break;
				}
				case SkyType::Skybox:
				default:
					ADRIA_ASSERT(false);
				}
				cmd_list->Dispatch(SKYCUBE_SIZE / 16, SKYCUBE_SIZE / 16, 6);

			}, RGPassType::Compute, RGPassFlags::ForceNoCull);
	}

	void SkyPass::AddDrawSkyPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<void>("Draw Sky Pass",
			[=](RenderGraphBuilder& builder)
			{
				std::ignore = builder.ReadTexture(RG_NAME(Sky));
				builder.WriteRenderTarget(RG_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.ReadDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				cmd_list->SetPipelineState(sky_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
				cmd_list->SetVertexBuffer(GfxVertexBufferView(cube_vb.get()));
				GfxIndexBufferView ibv(cube_ib.get());
				cmd_list->SetIndexBuffer(&ibv);
				cmd_list->DrawIndexed(cube_ib->GetCount());
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void SkyPass::OnSceneInitialized()
	{
		CreateCubeBuffers();
	}

	void SkyPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Sky Settings", ImGuiTreeNodeFlags_None))
				{
					static int current_sky_type = 2;
					if (ImGui::Combo("Sky Type", &current_sky_type, "Skybox\0Minimal Atmosphere\0Hosek-Wilkie\0", 3))
					{
						sky_type = static_cast<SkyType>(current_sky_type);
					}

					if (sky_type == SkyType::HosekWilkie)
					{
						sky_type = SkyType::HosekWilkie;
						ImGui::SliderFloat("Turbidity", &turbidity, 2.0f, 30.0f);
						ImGui::SliderFloat("Ground Albedo", &ground_albedo, 0.0f, 1.0f);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer
		);
	}

	void SkyPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	int32 SkyPass::GetSkyIndex() const
	{
		if (sky_type == SkyType::Skybox)
		{
			auto skybox_view = reg.view<Skybox>();
			for (auto e : skybox_view)
			{
				auto const& skybox = skybox_view.get<Skybox>(e);
				if (!skybox.active) continue;

				ADRIA_ASSERT(skybox.cubemap_texture != INVALID_TEXTURE_HANDLE);
				return (int32)skybox.cubemap_texture;
			}
		}

		GfxDescriptor sky_srv_gpu = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, sky_srv_gpu, sky_texture_srv);
		return (int32)sky_srv_gpu.GetIndex();
	}

	void SkyPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_MinimalAtmosphereSky;
		minimal_atmosphere_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_HosekWilkieSky;
		hosek_wilkie_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
		GfxReflection::FillInputLayoutDesc(GetGfxShader(VS_Sky), gfx_pso_desc.input_layout);
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_Sky;
		gfx_pso_desc.PS = PS_Sky;
		gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
		gfx_pso_desc.depth_state.depth_enable = true;
		gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
		gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		gfx_pso_desc.num_render_targets = 1;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
		gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
		sky_pso = gfx->CreateGraphicsPipelineState(gfx_pso_desc);
	}

	void SkyPass::CreateCubeBuffers()
	{
		GfxTextureDesc sky_desc{};
		sky_desc.type = GfxTextureType_2D;
		sky_desc.width = SKYCUBE_SIZE;
		sky_desc.height = SKYCUBE_SIZE;
		sky_desc.misc_flags = GfxTextureMiscFlag::TextureCube;
		sky_desc.array_size = 6;
		sky_desc.format = GfxFormat::R16G16B16A16_FLOAT;
		sky_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		sky_desc.initial_state = GfxResourceState::ComputeUAV;
		sky_texture = gfx->CreateTexture(sky_desc);

		GfxTextureDescriptorDesc sky_srv_desc{};
		sky_srv_desc.first_slice = 0;
		sky_srv_desc.slice_count = 6;
		sky_texture_srv = gfx->CreateTextureSRV(sky_texture.get(), &sky_srv_desc);

		SimpleVertex const cube_vertices[8] =
		{
			Vector3{ -0.5f, -0.5f,  0.5f },
			Vector3{  0.5f, -0.5f,  0.5f },
			Vector3{  0.5f,  0.5f,  0.5f },
			Vector3{ -0.5f,  0.5f,  0.5f },
			Vector3{ -0.5f, -0.5f, -0.5f },
			Vector3{  0.5f, -0.5f, -0.5f },
			Vector3{  0.5f,  0.5f, -0.5f },
			Vector3{ -0.5f,  0.5f, -0.5f }
		};

		uint16 const cube_indices[36] =
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
		cube_vb = gfx->CreateBuffer(vb_desc, cube_vertices);

		GfxBufferDesc ib_desc{};
		ib_desc.bind_flags = GfxBindFlag::None;
		ib_desc.format = GfxFormat::R16_UINT;
		ib_desc.stride = sizeof(uint16);
		ib_desc.size = sizeof(cube_indices);
		cube_ib = gfx->CreateBuffer(ib_desc, cube_indices);
	}

}