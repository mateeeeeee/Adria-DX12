#include "BokehPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"

#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/GfxLinearDynamicAllocator.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{
	struct Bokeh
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 Size;
		DirectX::XMFLOAT3 Color;
	};

	BokehPass::BokehPass(uint32 w, uint32 h)
		: width(w), height(h)
	{}

	void BokehPass::AddPass(RenderGraph& rendergraph, RGResourceName input)
	{
		AddGenerateBokehPass(rendergraph, input);
		AddDrawBokehPass(rendergraph, input);
		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Bokeh", 0))
				{
					static char const* const bokeh_types[] = { "HEXAGON", "OCTAGON", "CIRCLE", "CROSS" };
					static int bokeh_type = static_cast<int>(params.bokeh_type);
					ImGui::ListBox("Bokeh Type", &bokeh_type, bokeh_types, IM_ARRAYSIZE(bokeh_types));
					params.bokeh_type = static_cast<BokehType>(bokeh_type);

					ImGui::SliderFloat("Bokeh Blur Threshold", &params.bokeh_blur_threshold, 0.0f, 1.0f);
					ImGui::SliderFloat("Bokeh Lum Threshold", &params.bokeh_lum_threshold, 0.0f, 10.0f);
					ImGui::SliderFloat("Bokeh Color Scale", &params.bokeh_color_scale, 0.1f, 10.0f);
					ImGui::SliderFloat("Bokeh Max Size", &params.bokeh_radius_scale, 0.0f, 100.0f);
					ImGui::SliderFloat("Bokeh Fallout", &params.bokeh_fallout, 0.0f, 2.0f);

					ImGui::TreePop();
					ImGui::Separator();
				}
			});
	}

	void BokehPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void BokehPass::OnSceneInitialized(GfxDevice* gfx)
	{
		hex_bokeh_handle = TextureManager::Get().LoadTexture(L"Resources/Textures/bokeh/Bokeh_Hex.dds");
		oct_bokeh_handle = TextureManager::Get().LoadTexture(L"Resources/Textures/bokeh/Bokeh_Oct.dds");
		circle_bokeh_handle = TextureManager::Get().LoadTexture(L"Resources/Textures/bokeh/Bokeh_Circle.dds");
		cross_bokeh_handle = TextureManager::Get().LoadTexture(L"Resources/Textures/bokeh/Bokeh_Cross.dds");

		GfxBufferDesc reset_buffer_desc{};
		reset_buffer_desc.size = sizeof(uint32);
		reset_buffer_desc.resource_usage = GfxResourceUsage::Upload;
		uint32 initial_data[] = { 0 };
		counter_reset_buffer = std::make_unique<GfxBuffer>(gfx, reset_buffer_desc, initial_data);

		GfxBufferDesc buffer_desc{};
		buffer_desc.size = 4 * sizeof(uint32);
		buffer_desc.bind_flags = GfxBindFlag::None;
		buffer_desc.misc_flags = GfxBufferMiscFlag::IndirectArgs;
		buffer_desc.resource_usage = GfxResourceUsage::Default;

		uint32 init_data[] = { 0,1,0,0 };
		bokeh_indirect_buffer = std::make_unique<GfxBuffer>(gfx, buffer_desc, init_data);
	}


	void BokehPass::AddGenerateBokehPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		RGResourceName last_resource = input;
		struct BokehCounterResetPassData
		{
			RGBufferCopySrcId src;
			RGBufferCopyDstId dst;
		};

		rg.AddPass<BokehCounterResetPassData>("Bokeh Counter Reset Pass",
			[=](BokehCounterResetPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc bokeh_counter_desc{};
				bokeh_counter_desc.size = sizeof(uint32);
				builder.DeclareBuffer(RG_RES_NAME(BokehCounter), bokeh_counter_desc);
				data.dst = builder.WriteCopyDstBuffer(RG_RES_NAME(BokehCounter));
			},
			[=](BokehCounterResetPassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				GfxBuffer& bokeh_counter = context.GetCopyDstBuffer(data.dst);
				cmd_list->CopyBuffer(bokeh_counter, 0, *counter_reset_buffer, 0, sizeof(uint32));
			}, RGPassType::Copy, RGPassFlags::None);

		struct BokehGeneratePassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGBufferReadWriteId bokeh;
		};

		rg.AddPass<BokehGeneratePassData>("Bokeh Generate Pass",
			[=](BokehGeneratePassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc bokeh_desc{};
				bokeh_desc.resource_usage = GfxResourceUsage::Default;
				bokeh_desc.misc_flags = GfxBufferMiscFlag::BufferStructured;
				bokeh_desc.stride = sizeof(Bokeh);
				bokeh_desc.size = bokeh_desc.stride * width * height;
				builder.DeclareBuffer(RG_RES_NAME(Bokeh), bokeh_desc);
				data.bokeh = builder.WriteBuffer(RG_RES_NAME(Bokeh), RG_RES_NAME(BokehCounter));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](BokehGeneratePassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				GfxDescriptor dst_handle = descriptor_allocator->Allocate(3);
				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.input), context.GetReadOnlyTexture(data.depth), context.GetReadWriteBuffer(data.bokeh) };
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct BokehGenerationIndices
				{
					uint32 hdr_idx;
					uint32 depth_idx;
					uint32 bokeh_stack_idx;
				} indices =
				{
					.hdr_idx = i, .depth_idx = i + 1, .bokeh_stack_idx = i + 2
				};

				DoFBlackboardData const& dof_data = context.GetBlackboard().GetChecked<DoFBlackboardData>();
				struct BokehGenerationConstants
				{
					XMFLOAT4 dof_params;
					float  bokeh_lum_threshold;
					float  bokeh_blur_threshold;
					float  bokeh_scale;
					float  bokeh_fallout;
				} constants =
				{
					.dof_params = XMFLOAT4(dof_data.dof_params_x, dof_data.dof_params_y, dof_data.dof_params_z, dof_data.dof_params_w),
					.bokeh_lum_threshold = params.bokeh_lum_threshold, .bokeh_blur_threshold = params.bokeh_blur_threshold,
					.bokeh_scale = params.bokeh_radius_scale, .bokeh_fallout = params.bokeh_fallout
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BokehGenerate));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, indices);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);

			}, RGPassType::Compute, RGPassFlags::None);

		struct BokehCopyToIndirectBufferPass
		{
			RGBufferCopySrcId src;
			RGBufferCopyDstId dst;
		};
		rg.ImportBuffer(RG_RES_NAME(BokehIndirectDraw), bokeh_indirect_buffer.get());
		rg.AddPass<BokehCopyToIndirectBufferPass>("Bokeh Indirect Buffer Pass",
			[=](BokehCopyToIndirectBufferPass& data, RenderGraphBuilder& builder)
			{
				data.dst = builder.WriteCopyDstBuffer(RG_RES_NAME(BokehIndirectDraw));
				data.src = builder.ReadCopySrcBuffer(RG_RES_NAME(BokehCounter));
			},
			[=](BokehCopyToIndirectBufferPass const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				GfxBuffer const& src_buffer = context.GetCopySrcBuffer(data.src);
				GfxBuffer&		 dst_buffer = context.GetCopyDstBuffer(data.dst);
				cmd_list->CopyBuffer(dst_buffer, 0, src_buffer, 0, src_buffer.GetDesc().size);
			}, RGPassType::Copy, RGPassFlags::None);
	}
	void BokehPass::AddDrawBokehPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		RGResourceName last_resource = input;
		struct BokehDrawPassData
		{
			RGBufferReadOnlyId bokeh_stack;
			RGBufferIndirectArgsId bokeh_indirect_args;
		};

		rg.AddPass<BokehDrawPassData>("Bokeh Draw Pass",
			[=](BokehDrawPassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(last_resource, RGLoadStoreAccessOp::Preserve_Preserve);
				data.bokeh_stack = builder.ReadBuffer(RG_RES_NAME(Bokeh), ReadAccess_PixelShader);
				data.bokeh_indirect_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(BokehIndirectDraw));
				builder.SetViewport(width, height);
			},
			[=](BokehDrawPassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				GfxDescriptor bokeh_descriptor{};
				switch (params.bokeh_type)
				{
				case BokehType::Hex:
					bokeh_descriptor = TextureManager::Get().GetSRV(hex_bokeh_handle);
					break;
				case BokehType::Oct:
					bokeh_descriptor = TextureManager::Get().GetSRV(oct_bokeh_handle);
					break;
				case BokehType::Circle:
					bokeh_descriptor = TextureManager::Get().GetSRV(circle_bokeh_handle);
					break;
				case BokehType::Cross:
					bokeh_descriptor = TextureManager::Get().GetSRV(cross_bokeh_handle);
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Bokeh Type");
				}

				uint32 i = descriptor_allocator->Allocate(2).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i),bokeh_descriptor);
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), context.GetReadOnlyBuffer(data.bokeh_stack));

				struct BokehConstants
				{
					uint32 bokeh_tex_idx;
					uint32 bokeh_stack_idx;
				} constants =
				{
					.bokeh_tex_idx = i, .bokeh_stack_idx = i + 1
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Bokeh));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetVertexBuffers({});
				cmd_list->SetTopology(GfxPrimitiveTopology::PointList);

				GfxBuffer const& indirect_args_buffer = context.GetIndirectArgsBuffer(data.bokeh_indirect_args);
				cmd_list->DrawIndirect(indirect_args_buffer, 0);
			}, RGPassType::Graphics, RGPassFlags::None);

	}

}

