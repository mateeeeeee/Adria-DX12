#include "BokehPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	BokehPass::BokehPass(TextureManager& texture_manager, uint32 w, uint32 h)
		: texture_manager(texture_manager), width(w), height(h)
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
					params.bokeh_type = static_cast<EBokehType>(bokeh_type);

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

	void BokehPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		hex_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Hex.dds");
		oct_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Oct.dds");
		circle_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Circle.dds");
		cross_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Cross.dds");

		BufferDesc reset_buffer_desc{};
		reset_buffer_desc.size = sizeof(uint32);
		reset_buffer_desc.resource_usage = EResourceUsage::Upload;
		uint32 initial_data[] = { 0 };
		counter_reset_buffer = std::make_unique<Buffer>(gfx, reset_buffer_desc, initial_data);

		BufferDesc buffer_desc{};
		buffer_desc.size = 4 * sizeof(uint32);
		buffer_desc.bind_flags = EBindFlag::None;
		buffer_desc.misc_flags = EBufferMiscFlag::IndirectArgs;
		buffer_desc.resource_usage = EResourceUsage::Default;

		uint32 init_data[] = { 0,1,0,0 };
		bokeh_indirect_buffer = std::make_unique<Buffer>(gfx, buffer_desc, init_data);

		D3D12_INDIRECT_ARGUMENT_DESC args[1];
		args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
		D3D12_COMMAND_SIGNATURE_DESC command_signature_desc{};
		command_signature_desc.NumArgumentDescs = 1;
		command_signature_desc.pArgumentDescs = args;
		command_signature_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
		BREAK_IF_FAILED(gfx->GetDevice()->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&bokeh_command_signature)));
	}


	void BokehPass::AddGenerateBokehPass(RenderGraph& rg, RGResourceName input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
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
			[=](BokehCounterResetPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Buffer const& bokeh_counter = context.GetCopyDstBuffer(data.dst);
				cmd_list->CopyBufferRegion(bokeh_counter.GetNative(), 0, counter_reset_buffer->GetNative(), 0, sizeof(uint32));
			}, ERGPassType::Copy, ERGPassFlags::None);

		struct BokehGeneratePassData
		{
			RGTextureReadOnlyId input_srv;
			RGTextureReadOnlyId depth_srv;
			RGBufferReadWriteId bokeh_uav;
		};

		rg.AddPass<BokehGeneratePassData>("Bokeh Generate Pass",
			[=](BokehGeneratePassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc bokeh_desc{};
				bokeh_desc.resource_usage = EResourceUsage::Default;
				bokeh_desc.misc_flags = EBufferMiscFlag::BufferStructured;
				bokeh_desc.stride = sizeof(Bokeh);
				bokeh_desc.size = bokeh_desc.stride * width * height;
				builder.DeclareBuffer(RG_RES_NAME(Bokeh), bokeh_desc);
				data.bokeh_uav = builder.WriteBuffer(RG_RES_NAME(Bokeh), RG_RES_NAME(BokehCounter));
				data.input_srv = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](BokehGeneratePassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::BokehGenerate));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::BokehGenerate));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(1, global_data.postprocess_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(2, global_data.compute_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.input_srv), context.GetReadOnlyTexture(data.depth_srv) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1 };
				uint32 dst_range_sizes[] = { 2 };
				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));

				D3D12_CPU_DESCRIPTOR_HANDLE bokeh_uav = context.GetReadWriteBuffer(data.bokeh_uav);
				descriptor_index = descriptor_allocator->Allocate();

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), bokeh_uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->Dispatch((uint32)std::ceil(width / 32.0f), (uint32)std::ceil(height / 32.0f), 1);

			}, ERGPassType::Compute, ERGPassFlags::None);

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
			[=](BokehCopyToIndirectBufferPass const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Buffer const& src_buffer = context.GetCopySrcBuffer(data.src);
				Buffer const& dst_buffer = context.GetCopyDstBuffer(data.dst);
				cmd_list->CopyBufferRegion(dst_buffer.GetNative(), 0, src_buffer.GetNative(), 0, src_buffer.GetDesc().size);
			}, ERGPassType::Copy, ERGPassFlags::None);
	}
	void BokehPass::AddDrawBokehPass(RenderGraph& rg, RGResourceName input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = input;
		struct BokehDrawPassData
		{
			RGBufferReadOnlyId bokeh_srv;
			RGBufferIndirectArgsId bokeh_indirect_args;
		};

		rg.AddPass<BokehDrawPassData>("Bokeh Draw Pass",
			[=](BokehDrawPassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(last_resource, ERGLoadStoreAccessOp::Preserve_Preserve);
				data.bokeh_srv = builder.ReadBuffer(RG_RES_NAME(Bokeh), ReadAccess_PixelShader);
				data.bokeh_indirect_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(BokehIndirectDraw));
				builder.SetViewport(width, height);
			},
			[=](BokehDrawPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				D3D12_CPU_DESCRIPTOR_HANDLE bokeh_descriptor{};
				switch (params.bokeh_type)
				{
				case EBokehType::Hex:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(hex_bokeh_handle);
					break;
				case EBokehType::Oct:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(oct_bokeh_handle);
					break;
				case EBokehType::Circle:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(circle_bokeh_handle);
					break;
				case EBokehType::Cross:
					bokeh_descriptor = texture_manager.CpuDescriptorHandle(cross_bokeh_handle);
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Bokeh Type");
				}

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Bokeh));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Bokeh));

				OffsetType i = descriptor_allocator->AllocateRange(2);

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i),
					context.GetReadOnlyBuffer(data.bokeh_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1),
					bokeh_descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(i));
				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(i + 1));

				cmd_list->IASetVertexBuffers(0, 0, nullptr);
				cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

				Buffer const& indirect_args_buffer = context.GetIndirectArgsBuffer(data.bokeh_indirect_args);
				cmd_list->ExecuteIndirect(bokeh_command_signature.Get(), 1, indirect_args_buffer.GetNative(), 0,
					nullptr, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);

	}

}

