#include <format>
#include "BloomPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{
	BloomPass::BloomPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName BloomPass::AddPass(RenderGraph& rg, RGResourceName color_texture)
	{
		uint32 pass_count = (uint32)std::floor(log2f((float)std::max(width, height))) - 3;
		std::vector<RGResourceName> downsample_mips(pass_count);
		downsample_mips[0] = DownsamplePass(rg, color_texture, 1);
		for (uint32 i = 1; i < pass_count; ++i)
		{
			downsample_mips[i] = DownsamplePass(rg, downsample_mips[i - 1], i + 1);
		}

		std::vector<RGResourceName> upsample_mips(pass_count);
		upsample_mips[pass_count - 1] = downsample_mips[pass_count - 1];

		for (int32 i = pass_count - 2; i >= 0; --i)
		{
			upsample_mips[i] = UpsamplePass(rg, downsample_mips[i], upsample_mips[i + 1], i + 1);
		}

		BloomBlackboardData blackboard_data{ .bloom_intensity = params.bloom_intensity, .bloom_blend_factor = params.bloom_blend_factor };
		rg.GetBlackboard().Add<BloomBlackboardData>(std::move(blackboard_data));

		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Bloom", 0))
				{
					ImGui::SliderFloat("Bloom Radius", &params.radius, 0.0f, 1.0f);
					ImGui::SliderFloat("Bloom Intensity", &params.bloom_intensity, 0.0f, 8.0f);
					ImGui::SliderFloat("Bloom Blend Factor", &params.bloom_blend_factor, 0.0f, 1.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);

		return upsample_mips.front();
	}

	void BloomPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	RGResourceName BloomPass::DownsamplePass(RenderGraph& rg, RGResourceName input, uint32 pass_idx)
	{
		uint32 target_dim_x = std::max(1u, width >> pass_idx);
		uint32 target_dim_y = std::max(1u, height >> pass_idx);

		RGResourceName output = RG_RES_NAME_IDX(BloomDownsample, pass_idx);
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();

		struct BloomDownsamplePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId  input;
		};

		std::string pass_name = std::format("Bloom Downsample Pass {}", pass_idx);
		rg.AddPass<BloomDownsamplePassData>(pass_name.c_str(),
			[=](BloomDownsamplePassData& data, RenderGraphBuilder& builder)
			{
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);

				RGTextureDesc desc{};
				desc.width = target_dim_x;
				desc.height = target_dim_y;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(output, desc);
				data.output = builder.WriteTexture(output);
			},
			[=](BloomDownsamplePassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				uint32 i = descriptor_allocator->Allocate(2).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.input));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output));

				struct BloomDownsampleConstants
				{
					float    dims_inv_x;
					float    dims_inv_y;
					uint32   source_idx;
					uint32   target_idx;
				} constants =
				{
					.dims_inv_x = 1.0f / target_dim_x,
					.dims_inv_y = 1.0f / target_dim_y,
					.source_idx = i,
					.target_idx = i + 1
				};
				GfxPipelineStateID pso = pass_idx == 1 ? GfxPipelineStateID::BloomDownsample_FirstPass : GfxPipelineStateID::BloomDownsample;
				cmd_list->SetPipelineState(PSOCache::Get(pso));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(target_dim_x / 8.0f), (uint32)std::ceil(target_dim_y / 8.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		return output;
	}

	RGResourceName BloomPass::UpsamplePass(RenderGraph& rg, RGResourceName input_high, RGResourceName input_low, uint32 pass_idx)
	{
		struct BloomUpsamplePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId  input_low;
			RGTextureReadOnlyId  input_high;
		};

		uint32 target_dim_x = std::max(1u, width >> pass_idx);
		uint32 target_dim_y = std::max(1u, height >> pass_idx);

		RGResourceName output = pass_idx != 1 ? RG_RES_NAME_IDX(BloomUpsample, pass_idx) : RG_RES_NAME(Bloom);
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();

		std::string pass_name = std::format("Bloom Upsample Pass {}", pass_idx);
		rg.AddPass<BloomUpsamplePassData>(pass_name.c_str(),
			[=](BloomUpsamplePassData& data, RenderGraphBuilder& builder)
			{
				data.input_high = builder.ReadTexture(input_high);
				data.input_low  = builder.ReadTexture(input_low);

				RGTextureDesc desc{};
				desc.width = target_dim_x;
				desc.height = target_dim_y;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(output, desc);
				data.output = builder.WriteTexture(output);
			},
			[=](BloomUpsamplePassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				uint32 i = descriptor_allocator->Allocate(3).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.input_low));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.input_high));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadWriteTexture(data.output));

				struct BloomUpsampleConstants
				{
					float    dims_inv_x;
					float    dims_inv_y;
					uint32   low_input_idx;
					uint32   high_input_idx;
					uint32   output_idx;
					float    radius;
				} constants =
				{
					.dims_inv_x = 1.0f / (target_dim_x),
					.dims_inv_y = 1.0f / (target_dim_y),
					.low_input_idx = i,
					.high_input_idx = i + 1,
					.output_idx = i + 2,
					.radius = params.radius
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BloomUpsample));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(target_dim_x / 8.0f), (uint32)std::ceil(target_dim_y / 8.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);


		return output;
	}

}

