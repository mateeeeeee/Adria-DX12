#include "VolumetricLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxPipelineState.h"
#include "Editor/GUICommand.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"


using namespace DirectX;

namespace adria
{

	VolumetricLightingPass::VolumetricLightingPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), copy_to_texture_pass(gfx, w, h)
	{
		CreatePSOs();
	}

	void VolumetricLightingPass::AddPass(RenderGraph& rendergraph)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		rendergraph.AddPass<LightingPassData>("Volumetric Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc volumetric_output_desc{};
				volumetric_output_desc.width = width >> resolution;
				volumetric_output_desc.height = height >> resolution;
				volumetric_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(VolumetricLightOutput), volumetric_output_desc);

				data.output = builder.WriteTexture(RG_NAME(VolumetricLightOutput));
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);

				for (auto& shadow_texture : shadow_textures) std::ignore = builder.ReadTexture(shadow_texture);
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.depth),
												context.GetReadWriteTexture(data.output) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct VolumetricLightingConstants
				{
					uint32 depth_idx;
					uint32 output_idx;
					uint32 resolution_scale;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1, .resolution_scale = (uint32)resolution
				};
				
				cmd_list->SetPipelineState(volumetric_lighting_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp((width >> resolution), 16), DivideAndRoundUp((height >> resolution), 16), 1);
			}, RGPassType::Compute);

		copy_to_texture_pass.AddPass(rendergraph, RG_NAME(HDR_RenderTarget), RG_NAME(VolumetricLightOutput), BlendMode::AdditiveBlend);

		shadow_textures.clear();
	}

	void VolumetricLightingPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Volumetric Lighting", ImGuiTreeNodeFlags_None))
				{
					static int _resolution = (int)resolution;
					if (ImGui::Combo("Volumetric Lighting Resolution", &_resolution, "Full\0Half\0Quarter\0", 3))
					{
						resolution = (VolumetricLightingResolution)_resolution;
						OnResize(width, height);
					}

					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer);
	}

	void VolumetricLightingPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_VolumetricLighting;
		volumetric_lighting_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}




