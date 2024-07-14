#include "BlurPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"


namespace adria
{

	BlurPass::BlurPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}

	BlurPass::~BlurPass() = default;

	void BlurPass::AddPass(RenderGraph& rendergraph, RGResourceName src_texture, RGResourceName blurred_texture,
		char const* pass_name)
	{
		struct BlurPassData
		{
			RGTextureReadOnlyId src_texture;
			RGTextureReadWriteId dst_texture;
		};

		static uint64 counter = 0;
		counter++;

		std::string name = "Horizontal Blur Pass" + std::string(pass_name);
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		rendergraph.AddPass<BlurPassData>(name.c_str(),
			[=](BlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc blur_desc = builder.GetTextureDesc(src_texture);

				builder.DeclareTexture(RG_RES_NAME_IDX(Intermediate, counter), blur_desc);
				data.dst_texture = builder.WriteTexture(RG_RES_NAME_IDX(Intermediate, counter));
				data.src_texture = builder.ReadTexture(src_texture, ReadAccess_NonPixelShader);
				builder.SetViewport(width, height);
			},
			[=](BlurPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					context.GetReadOnlyTexture(data.src_texture),
					context.GetReadWriteTexture(data.dst_texture)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct BlurConstants
				{
					uint32 input_idx;
					uint32 output_idx;
				} constants =
				{
					.input_idx = i, .output_idx = i + 1
				};

				cmd_list->SetPipelineState(blur_horizontal_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 1024), height, 1);
			}, RGPassType::Compute, RGPassFlags::None);

		name = "Vertical Blur Pass" + std::string(pass_name);
		rendergraph.AddPass<BlurPassData>(name.c_str(),
			[=](BlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc blur_desc = builder.GetTextureDesc(src_texture);
				builder.DeclareTexture(blurred_texture, blur_desc);
				data.dst_texture = builder.WriteTexture(blurred_texture);
				data.src_texture = builder.ReadTexture(RG_RES_NAME_IDX(Intermediate, counter), ReadAccess_NonPixelShader);
			},
			[=](BlurPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					context.GetReadOnlyTexture(data.src_texture),
					context.GetReadWriteTexture(data.dst_texture)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct BlurConstants
				{
					uint32 input_idx;
					uint32 output_idx;
				} constants =
				{
					.input_idx = i, .output_idx = i + 1
				};

				cmd_list->SetPipelineState(blur_vertical_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1,constants);
				cmd_list->Dispatch(width, DivideAndRoundUp(height, 1024), 1);

			}, RGPassType::Compute, RGPassFlags::None);

	}

	void BlurPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void BlurPass::SetResolution(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void BlurPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Blur_Horizontal;
		blur_horizontal_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_Blur_Vertical;
		blur_vertical_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}