#include "FogPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "RenderGraph/RenderGraph.h"
#include "TextureManager.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Logging/Logger.h"
#include "Editor/GUICommand.h"
#include "Math/Packing.h"

namespace adria
{
	FogPass::FogPass(uint32 w, uint32 h)
		: width(w), height(h) {}

	RGResourceName FogPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		RGResourceName last_resource = input;
		struct FogPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId input;
			RGTextureReadWriteId output;
		};

		rg.AddPass<FogPassData>("Fog Pass",
			[=](FogPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc fog_output_desc{};
				fog_output_desc.width = width;
				fog_output_desc.height = height;
				fog_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(FogOutput), fog_output_desc);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
				data.output = builder.WriteTexture(RG_RES_NAME(FogOutput));
				builder.SetViewport(width, height);
			},
			[=](FogPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				uint32 i = descriptor_allocator->Allocate(3).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.input));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadWriteTexture(data.output));

				struct FogConstants
				{
					float fog_falloff;
					float fog_density;
					float fog_start;
					uint32  fog_color;
					uint32  fog_type;

					uint32  depth_idx;
					uint32  scene_idx;
					uint32  output_idx;
				} constants = 
				{
					.fog_falloff = params.fog_falloff, .fog_density = params.fog_density, .fog_start = params.fog_start,
					.fog_color = PackToUint(params.fog_color), .fog_type = static_cast<uint32>(params.fog_type),
					.depth_idx = i, .scene_idx = i + 1, .output_idx = i + 2
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Fog));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		AddGUI([&]() 
			{
				if(ImGui::TreeNodeEx("Fog", 0))
				{
					static const char* fog_types[] = { "Exponential", "Exponential Height" };
					static int current_fog_type = 0; // Here we store our selection data as an index.
					const char* combo_label = fog_types[current_fog_type];  // Label to preview before opening the combo (technically it could be anything)
					if (ImGui::BeginCombo("Fog Type", combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(fog_types); n++)
						{
							const bool is_selected = (current_fog_type == n);
							if (ImGui::Selectable(fog_types[n], is_selected)) current_fog_type = n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					params.fog_type = static_cast<FogType>(current_fog_type);

					ImGui::SliderFloat("Fog Falloff", &params.fog_falloff, 0.0001f, 0.01f);
					ImGui::SliderFloat("Fog Density", &params.fog_density, 0.0001f, 0.01f);
					ImGui::SliderFloat("Fog Start", &params.fog_start, 0.1f, 10000.0f);
					ImGui::ColorEdit3("Fog Color", params.fog_color);

					ImGui::TreePop();
					ImGui::Separator();
				}
			});
		return RG_RES_NAME(FogOutput);
	}

	void FogPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}
}


