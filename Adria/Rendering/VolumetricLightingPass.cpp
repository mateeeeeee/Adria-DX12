#include "VolumetricLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Editor/GUICommand.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"


using namespace DirectX;

namespace adria
{

	void VolumetricLightingPass::AddPass(RenderGraph& rendergraph)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();
		rendergraph.AddPass<LightingPassData>("Volumetric Lighting Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc volumetric_output_desc{};
				volumetric_output_desc.width = width >> resolution;
				volumetric_output_desc.height = height >> resolution;
				volumetric_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(VolumetricLightOutput), volumetric_output_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(VolumetricLightOutput));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
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
				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::VolumetricLighting));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil((width >> resolution) / 16.0f), (uint32)std::ceil((height >> resolution) / 16.0f), 1);
			}, RGPassType::Compute);

		copy_to_texture_pass.AddPass(rendergraph, RG_RES_NAME(HDR_RenderTarget), RG_RES_NAME(VolumetricLightOutput), BlendMode::AdditiveBlend);

		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Volumetric Lighting", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					static int _resolution = 1;
					static const char* res_types[] = { "Full", "Half", "Quarter" };
					const char* res_combo_label = res_types[_resolution];
					if (ImGui::BeginCombo("Volumetric Lighting Resolution", res_combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(res_types); n++)
						{
							const bool is_selected = (_resolution == n);
							if (ImGui::Selectable(res_types[n], is_selected)) _resolution = (VolumetricLightingResolution)n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (resolution != _resolution)
					{
						resolution = (VolumetricLightingResolution)_resolution;
						OnResize(width, height);
					}

					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

}




