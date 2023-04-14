#include "DecalsPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "RenderGraph/RenderGraph.h"
#include "TextureManager.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	DecalsPass::DecalsPass(entt::registry& reg, uint32 w, uint32 h)
	 : reg{ reg }, width{ w }, height{ h }
	{}

	void DecalsPass::AddPass(RenderGraph& rendergraph)
	{
		if (reg.view<Decal>().size() == 0) return;
		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();

		struct DecalsPassData
		{
			RGTextureReadOnlyId depth_srv;
		};
		rendergraph.AddPass<DecalsPassData>("Decals Pass",
			[=](DecalsPassData& data, RenderGraphBuilder& builder)
			{
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferNormal), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](DecalsPassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				
				GfxDescriptor depth_srv = descriptor_allocator->Allocate();
				gfx->CopyDescriptors(1, depth_srv, context.GetReadOnlyTexture(data.depth_srv));

				uint32 depth_idx = depth_srv.GetIndex();
				struct DecalsConstants
				{
					XMMATRIX model_matrix;
					XMMATRIX transposed_inverse_model;
					uint32 decal_type;
					uint32 decal_albedo_idx;
					uint32 decal_normal_idx;
					uint32 depth_idx;
				} constants = 
				{
					.depth_idx = depth_idx
				};

				
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				auto decal_view = reg.view<Decal>();

				auto decal_pass_lambda = [&](bool modify_normals)
				{
					if (decal_view.empty()) return;
					cmd_list->SetPipelineState(PSOCache::Get(modify_normals ? GfxPipelineStateID::Decals_ModifyNormals : GfxPipelineStateID::Decals));
					for (auto e : decal_view)
					{
						Decal& decal = decal_view.get<Decal>(e);
						if (decal.modify_gbuffer_normals != modify_normals) continue;

						constants.model_matrix = decal.decal_model_matrix;
						constants.transposed_inverse_model = XMMatrixTranspose(XMMatrixInverse(nullptr, decal.decal_model_matrix));
						constants.decal_type = static_cast<uint32>(decal.decal_type);
						constants.decal_albedo_idx = (uint32)decal.albedo_decal_texture;
						constants.decal_normal_idx = (uint32)decal.normal_decal_texture;
						
						cmd_list->SetRootCBV(2, constants);
						cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
						BindVertexBuffer(cmd_list->GetNative(), cube_vb.get());
						BindIndexBuffer(cmd_list->GetNative(), cube_ib.get());
						cmd_list->DrawIndexed(cube_ib->GetCount());
					}
				};
				decal_pass_lambda(false);
				decal_pass_lambda(true);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void DecalsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void DecalsPass::OnSceneInitialized(GfxDevice* gfx)
	{
		CreateCubeBuffers(gfx);
	}

	void DecalsPass::CreateCubeBuffers(GfxDevice* gfx)
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

