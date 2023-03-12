#include "ToneMapPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../Graphics/GraphicsCommon.h"
#include "../Graphics/TextureManager.h"
#include "../Math/Packing.h"
#include "../Editor/GUICommand.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	ToneMapPass::ToneMapPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void ToneMapPass::AddPass(RenderGraph& rg, RGResourceName hdr_src)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		BloomBlackboardData const* bloom_data  = rg.GetBlackboard().Get<BloomBlackboardData>();
		
		struct ToneMapPassData
		{
			RGTextureReadOnlyId  hdr_input;
			RGTextureReadOnlyId  exposure;
			RGTextureReadOnlyId  bloom;
			RGTextureReadWriteId output;
		};

		rg.AddPass<ToneMapPassData>("Tonemap Pass",
			[=](ToneMapPassData& data, RenderGraphBuilder& builder)
			{
				data.hdr_input = builder.ReadTexture(hdr_src, ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_RES_NAME(Exposure))) data.exposure = builder.ReadTexture(RG_RES_NAME(Exposure), ReadAccess_NonPixelShader);
				else data.exposure.Invalidate();

				if (builder.IsTextureDeclared(RG_RES_NAME(Bloom))) 
					data.bloom = builder.ReadTexture(RG_RES_NAME(Bloom), ReadAccess_NonPixelShader);
				else data.bloom.Invalidate();

				ADRIA_ASSERT(builder.IsTextureDeclared(RG_RES_NAME(FinalTexture)));
				data.output = builder.WriteTexture(RG_RES_NAME(FinalTexture));
			},
			[=](ToneMapPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(4);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.hdr_input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), 
					data.exposure.IsValid() ? ctx.GetReadOnlyTexture(data.exposure) : gfxcommon::GetCommonTexture(ECommonTextureType::WhiteTexture2D)->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				bool const bloom_enabled = data.bloom.IsValid();
				if (bloom_enabled) device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadOnlyTexture(data.bloom), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct TonemapConstants
				{
					float    tonemap_exposure;
					uint32   tonemap_operator;
					uint32   hdr_idx;
					uint32   exposure_idx;
					uint32   output_idx;
					int32    bloom_idx;
					uint32   lens_dirt_idx;
					uint32   bloom_params_packed;
				} constants = 
				{
					.tonemap_exposure = params.tonemap_exposure, .tonemap_operator = static_cast<uint32>(params.tone_map_op),
					.hdr_idx = i, .exposure_idx = i + 1, .output_idx = i + 2, .bloom_idx = -1
				};
				if (bloom_enabled)
				{
					ADRIA_ASSERT(bloom_data != nullptr);
					constants.bloom_idx = i + 3;
					constants.lens_dirt_idx = (uint32)lens_dirt_handle;
					constants.bloom_params_packed = PackTwoFloatsToUint32(bloom_data->bloom_intensity, bloom_data->bloom_blend_factor);
				}
				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ToneMap));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 8, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		GUI();
	}

	void ToneMapPass::AddPass(RenderGraph& rg, RGResourceName hdr_src, RGResourceName output)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		BloomBlackboardData const* bloom_data  = rg.GetBlackboard().Get<BloomBlackboardData>();

		ERGPassFlags flags = ERGPassFlags::None;

		struct ToneMapPassData
		{
			RGTextureReadOnlyId  hdr_input;
			RGTextureReadOnlyId  exposure;
			RGTextureReadOnlyId  bloom;
			RGTextureReadWriteId output;
		};

		rg.AddPass<ToneMapPassData>("Tonemap Pass",
			[=](ToneMapPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc fxaa_input_desc{};
				fxaa_input_desc.width = width;
				fxaa_input_desc.height = height;
				fxaa_input_desc.format = EFormat::R10G10B10A2_UNORM;
				builder.DeclareTexture(output, fxaa_input_desc);

				data.hdr_input = builder.ReadTexture(hdr_src, ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_RES_NAME(Exposure))) data.exposure = builder.ReadTexture(RG_RES_NAME(Exposure), ReadAccess_NonPixelShader);
				else data.exposure.Invalidate();

				if (builder.IsTextureDeclared(RG_RES_NAME(Bloom))) 
					data.bloom = builder.ReadTexture(RG_RES_NAME(Bloom), ReadAccess_NonPixelShader);
				else data.bloom.Invalidate();

				data.output = builder.WriteTexture(output);
				builder.SetViewport(width, height);
			},
			[=](ToneMapPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(4);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.hdr_input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), 
					data.exposure.IsValid() ? ctx.GetReadOnlyTexture(data.exposure) : gfxcommon::GetCommonTexture(ECommonTextureType::WhiteTexture2D)->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				bool const bloom_enabled = data.bloom.IsValid();
				if (bloom_enabled) device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadOnlyTexture(data.bloom), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct TonemapConstants
				{
					float    tonemap_exposure;
					uint32   tonemap_operator;
					uint32   hdr_idx;
					uint32   exposure_idx;
					uint32   output_idx;
					int32    bloom_idx;
					uint32   lens_dirt_idx;
					uint32   bloom_params_packed;
				} constants =
				{
					.tonemap_exposure = params.tonemap_exposure, .tonemap_operator = static_cast<uint32>(params.tone_map_op),
					.hdr_idx = i, .exposure_idx = i + 1, .output_idx = i + 2, .bloom_idx = -1
				};
				if (bloom_enabled)
				{
					ADRIA_ASSERT(bloom_data != nullptr);
					constants.bloom_idx = i + 3;
					constants.lens_dirt_idx = (uint32)lens_dirt_handle;
					constants.bloom_params_packed = PackTwoFloatsToUint32(bloom_data->bloom_intensity, bloom_data->bloom_blend_factor);
				}
				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ToneMap));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 8, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, flags);

		GUI();
	}

	void ToneMapPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void ToneMapPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		lens_dirt_handle = TextureManager::Get().LoadTexture(L"Resources/Textures/LensDirt.dds");
	}

	void ToneMapPass::GUI()
	{
		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("Tone Mapping", 0))
				{
					ImGui::SliderFloat("Exposure", &params.tonemap_exposure, 0.01f, 10.0f);
					static char const* const operators[] = { "REINHARD", "HABLE", "LINEAR" };
					static int tone_map_operator = static_cast<int>(params.tone_map_op);
					ImGui::ListBox("Tone Map Operator", &tone_map_operator, operators, IM_ARRAYSIZE(operators));
					params.tone_map_op = static_cast<EToneMap>(tone_map_operator);
					ImGui::TreePop();
					ImGui::Separator();
				}
			});
	}
}