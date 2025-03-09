#include <map>
#include "GBufferPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "RendererDebugViewPass.h"
#include "Graphics/GfxReflection.h"
#include "Graphics/GfxTracyProfiler.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	GBufferPass::GBufferPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h) :
		reg{ reg }, gfx{ gfx }, width{ w }, height{ h }
	{
		CreatePSOs();
	}

	GBufferPass::~GBufferPass() = default;

	void GBufferPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<void>("GBuffer Pass",
			[=](RenderGraphBuilder& builder)
			{
				RGTextureDesc gbuffer_desc{};
				gbuffer_desc.width = width;
				gbuffer_desc.height = height;
				gbuffer_desc.format = GfxFormat::R8G8B8A8_UNORM;
				gbuffer_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_NAME(GBufferNormal), gbuffer_desc);
				builder.DeclareTexture(RG_NAME(GBufferAlbedo), gbuffer_desc);
				builder.DeclareTexture(RG_NAME(GBufferEmissive), gbuffer_desc);
				builder.DeclareTexture(RG_NAME(GBufferCustom), gbuffer_desc);

				builder.WriteRenderTarget(RG_NAME(GBufferNormal), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_NAME(GBufferAlbedo), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_NAME(GBufferEmissive), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_NAME(GBufferCustom), RGLoadStoreAccessOp::Clear_Preserve);

				RGTextureDesc depth_desc{};
				depth_desc.width = width;
				depth_desc.height = height;
				depth_desc.format = GfxFormat::R32_TYPELESS;
				depth_desc.clear_value = GfxClearValue(0.0f, 0);
				builder.DeclareTexture(RG_NAME(DepthStencil), depth_desc);
				builder.WriteDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				reg.sort<Batch>([&frame_data](Batch const& lhs, Batch const& rhs)
					{ 
						if(lhs.alpha_mode != rhs.alpha_mode) return lhs.alpha_mode < rhs.alpha_mode;
						if(lhs.shading_extension != rhs.shading_extension)  return lhs.shading_extension < rhs.shading_extension;
						Vector3 camera_position(frame_data.camera_position);
						Float lhs_distance = Vector3::DistanceSquared(camera_position, lhs.bounding_box.Center);
						Float rhs_distance = Vector3::DistanceSquared(camera_position, rhs.bounding_box.Center);
						return lhs_distance < rhs_distance;
					});

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxShadingRateInfo const& vrs = gfx->GetVRSInfo();
				cmd_list->BeginVRS(vrs);

				if (skip_alpha_blended) ProcessBatches(reg.view<Batch>(entt::exclude_t<Transparent>()), cmd_list);
				else ProcessBatches(reg.view<Batch>(), cmd_list);

				cmd_list->EndVRS(vrs);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void GBufferPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	void GBufferPass::OnDebugViewChanged(RendererDebugView renderer_output)
	{
		debug_mipmaps = (renderer_output == RendererDebugView::ViewMipMaps);
		triangle_overdraw = (renderer_output == RendererDebugView::TriangleOverdraw);
		material_ids = (renderer_output == RendererDebugView::MaterialID);
	}

	void GBufferPass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc gbuffer_pso_desc{};
		GfxReflection::FillInputLayoutDesc(GetGfxShader(VS_GBuffer), gbuffer_pso_desc.input_layout);
		gbuffer_pso_desc.root_signature = GfxRootSignatureID::Common;
		gbuffer_pso_desc.VS = VS_GBuffer;
		gbuffer_pso_desc.PS = PS_GBuffer;
		gbuffer_pso_desc.depth_state.depth_enable = true;
		gbuffer_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		gbuffer_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		gbuffer_pso_desc.num_render_targets = 4u;
		gbuffer_pso_desc.rtv_formats[0] = GfxFormat::R8G8B8A8_UNORM;
		gbuffer_pso_desc.rtv_formats[1] = GfxFormat::R8G8B8A8_UNORM;
		gbuffer_pso_desc.rtv_formats[2] = GfxFormat::R8G8B8A8_UNORM;
		gbuffer_pso_desc.rtv_formats[3] = GfxFormat::R8G8B8A8_UNORM;
		gbuffer_pso_desc.dsv_format = GfxFormat::D32_FLOAT;

		gbuffer_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(gfx, gbuffer_pso_desc);
	}

	template<typename View>
	void GBufferPass::ProcessBatches(View view, GfxCommandList* cmd_list)
	{
		auto GetPSO = [this](ShadingExtension extension, MaterialAlphaMode alpha_mode)
			{
				using enum GfxShaderStage;
				if (raining) gbuffer_psos->AddDefine<PS>("RAIN", "1");
				if (debug_mipmaps) gbuffer_psos->AddDefine<PS>("VIEW_MIPMAPS", "1");
				if (triangle_overdraw) gbuffer_psos->AddDefine<PS>("TRIANGLE_OVERDRAW", "1");
				if (material_ids) gbuffer_psos->AddDefine<PS>("MATERIAL_ID", "1");

				switch (extension)
				{
				case ShadingExtension::Anisotropy:	gbuffer_psos->AddDefine<PS>("SHADING_EXTENSION_ANISOTROPY", "1"); break;
				case ShadingExtension::ClearCoat:	gbuffer_psos->AddDefine<PS>("SHADING_EXTENSION_CLEARCOAT", "1"); break;
				case ShadingExtension::Sheen:		gbuffer_psos->AddDefine<PS>("SHADING_EXTENSION_SHEEN", "1"); break;
				}

				switch (alpha_mode)
				{
				case MaterialAlphaMode::Opaque: break;
				case MaterialAlphaMode::Mask:   gbuffer_psos->AddDefine<PS>("MASK", "1"); break;
				case MaterialAlphaMode::Blend:  gbuffer_psos->SetCullMode(GfxCullMode::None); break;
				}
				return gbuffer_psos->Get();
			};

		for (auto batch_entity : view)
		{
			Batch& batch = view.get<Batch>(batch_entity);
			if (!batch.camera_visibility) continue;

			GfxPipelineState* pso = GetPSO(batch.shading_extension, batch.alpha_mode);
			cmd_list->SetPipelineState(pso);

			struct GBufferConstants
			{
				Uint32 instance_id;
			} constants{ .instance_id = batch.instance_id };
			cmd_list->SetRootConstants(1, constants);

			GfxIndexBufferView ibv(batch.submesh->buffer_address + batch.submesh->indices_offset, batch.submesh->indices_count);
			cmd_list->SetTopology(batch.submesh->topology);
			cmd_list->SetIndexBuffer(&ibv);
			cmd_list->DrawIndexed(batch.submesh->indices_count);
		}
	}
}

