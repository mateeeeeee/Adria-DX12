#include "Postprocessor.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "../tecs/registry.h"

//add array of ResourceNames of Postprocess

namespace adria
{
	Postprocessor::Postprocessor(TextureManager& texture_manager, uint32 width, uint32 height)
		: texture_manager(texture_manager), width(width), height(height),
		  blur_pass(width, height), copy_to_texture_pass(width, height), generate_mips_pass(width, height)
	{
	}

	void Postprocessor::AddPasses(RenderGraph& rg, PostprocessSettings const& settings)
	{
		AddCopyHDRPass(rg);
		final_resource = RG_RES_NAME(PostprocessMain);
		if (settings.clouds)
		{
			AddVolumetricCloudsPass(rg);
			blur_pass.AddPass(rg, RG_RES_NAME(CloudsOutput), RG_RES_NAME(BlurredCloudsOutput), "Volumetric Clouds");
			copy_to_texture_pass.AddPass(rg, RG_RES_NAME(PostprocessMain), RG_RES_NAME(BlurredCloudsOutput), EBlendMode::AlphaBlend);
		}

		if (settings.reflections == EReflections::SSR)
		{
			AddSSRPass(rg);
			final_resource = RG_RES_NAME(SSR_Output);
		}
		if (settings.fog)
		{
			AddFogPass(rg);
			final_resource = RG_RES_NAME(FogOutput);
		}
		if (settings.dof)
		{
			blur_pass.AddPass(rg, final_resource, RG_RES_NAME(BlurredDofInput), "DoF");
			//if (settings.bokeh) PassGenerateBokeh(cmd_list);
			AddDepthOfFieldPass(rg);
			final_resource = RG_RES_NAME(DepthOfFieldOutput);
		}
		if (settings.bloom)
		{
			AddBloomPass(rg);
			final_resource = RG_RES_NAME(BloomOutput);
		}
	}

	void Postprocessor::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(width, height);
		copy_to_texture_pass.OnResize(width, height);
		generate_mips_pass.OnResize(w, h);
	}

	void Postprocessor::OnSceneInitialized()
	{
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\weather.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\worley.dds"));
	}

	RGResourceName Postprocessor::GetFinalResource() const
	{
		return final_resource;
	}

	void Postprocessor::AddCopyHDRPass(RenderGraph& rg)
	{
		struct CopyPassData
		{
			RGTextureCopySrcId copy_src;
			RGTextureCopyDstId copy_dst;
		};

		rg.AddPass<CopyPassData>("Copy HDR Pass",
			[=](CopyPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc postprocess_desc{};
				postprocess_desc.width = width;
				postprocess_desc.height = height;
				postprocess_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				postprocess_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				postprocess_desc.initial_state = D3D12_RESOURCE_STATE_COPY_DEST;

				builder.DeclareTexture(RG_RES_NAME(PostprocessMain), postprocess_desc);
				data.copy_dst = builder.WriteCopyDstTexture(RG_RES_NAME(PostprocessMain));
				data.copy_src = builder.ReadCopySrcTexture(RG_RES_NAME(HDR_RenderTarget));
			},
			[=](CopyPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				Texture const& src_texture = resources.GetCopyResource(data.copy_src);
				Texture const& dst_texture = resources.GetCopyResource(data.copy_dst);
				cmd_list->CopyResource(dst_texture.GetNative(), src_texture.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::None);
	}

	void Postprocessor::AddVolumetricCloudsPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct VolumetricCloudsPassData
		{
			RGTextureReadOnlyId depth;
		};
		rg.AddPass<VolumetricCloudsPassData>("Volumetric Clouds Pass",
			[=](VolumetricCloudsPassData& data, RenderGraphBuilder& builder)
			{
				D3D12_CLEAR_VALUE rtv_clear_value{};
				rtv_clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				rtv_clear_value.Color[0] = 0.0f;
				rtv_clear_value.Color[1] = 0.0f;
				rtv_clear_value.Color[2] = 0.0f;
				rtv_clear_value.Color[3] = 0.0f;

				TextureDesc clouds_output_desc{};
				clouds_output_desc.clear = rtv_clear_value;
				clouds_output_desc.width = width;
				clouds_output_desc.height = height;
				clouds_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				clouds_output_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				clouds_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				builder.DeclareTexture(RG_RES_NAME(CloudsOutput), clouds_output_desc);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(CloudsOutput), ERGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=](VolumetricCloudsPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Clouds));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Clouds));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.weather_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(4);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { texture_manager.CpuDescriptorHandle(cloud_textures[0]),  texture_manager.CpuDescriptorHandle(cloud_textures[1]),
															 texture_manager.CpuDescriptorHandle(cloud_textures[2]), resources.GetDescriptor(data.depth) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1, 1, 1 };
				uint32 dst_range_sizes[] = { 4 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void Postprocessor::AddSSRPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = final_resource;
		struct SSRPassData
		{
			RGTextureReadOnlyId normals;
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<SSRPassData>("SSR Pass",
			[=](SSRPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc ssr_output_desc{};
				ssr_output_desc.width = width;
				ssr_output_desc.height = height;
				ssr_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				ssr_output_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				ssr_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				builder.DeclareTexture(RG_RES_NAME(SSR_Output), ssr_output_desc);
				builder.WriteRenderTarget(RG_RES_NAME(SSR_Output), ERGLoadStoreAccessOp::Discard_Preserve);
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.normals = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](SSRPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::SSR));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::SSR));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { resources.GetDescriptor(data.normals), resources.GetDescriptor(data.input), resources.GetDescriptor(data.depth) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1, 1 };
				uint32 dst_range_sizes[] = { 3 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void Postprocessor::AddFogPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = final_resource;
		struct FogPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId input;
		};

		rg.AddPass<FogPassData>("Fog Pass",
			[=](FogPassData& data, RenderGraphBuilder& builder)
			{
				D3D12_CLEAR_VALUE rtv_clear_value{};
				rtv_clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				rtv_clear_value.Color[0] = 0.0f;
				rtv_clear_value.Color[1] = 0.0f;
				rtv_clear_value.Color[2] = 0.0f;
				rtv_clear_value.Color[3] = 0.0f;

				TextureDesc fog_output_desc{};
				fog_output_desc.clear = rtv_clear_value;
				fog_output_desc.width = width;
				fog_output_desc.height = height;
				fog_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				fog_output_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource | EBindFlag::UnorderedAccess;
				fog_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				builder.DeclareTexture(RG_RES_NAME(FogOutput), fog_output_desc);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(FogOutput), ERGLoadStoreAccessOp::Discard_Preserve);
				builder.SetViewport(width, height);
			},
			[=](FogPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Fog));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Fog));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { resources.GetDescriptor(data.input),  resources.GetDescriptor(data.depth) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1 };
				uint32 dst_range_sizes[] = { 2 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void Postprocessor::AddBloomPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = final_resource;
		struct BloomExtractPassData
		{
			RGTextureReadWriteId extract;
			RGTextureReadOnlyId input;
		};

		rg.AddPass<BloomExtractPassData>("BloomExtract Pass",
			[=](BloomExtractPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc bloom_extract_desc{};
				bloom_extract_desc.width = width;
				bloom_extract_desc.height = height;
				bloom_extract_desc.mip_levels = 5;
				bloom_extract_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
				bloom_extract_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				bloom_extract_desc.initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; 

				builder.DeclareTexture(RG_RES_NAME(BloomExtract), bloom_extract_desc);
				data.extract = builder.WriteTexture(RG_RES_NAME(BloomExtract));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
			},
			[=](BloomExtractPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::BloomExtract));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::BloomExtract));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = resources.GetDescriptor(data.input);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				++descriptor_index;
				cpu_descriptor = resources.GetDescriptor(data.extract);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->Dispatch((uint32)std::ceil(width / 32.0f),
					(uint32)std::ceil(height / 32.0f), 1);

			}, ERGPassType::Compute, ERGPassFlags::None);

		generate_mips_pass.AddPass(rg, RG_RES_NAME(BloomExtract));

		struct BloomCombinePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId  input;
			RGTextureReadOnlyId  extract;
		};
		rg.AddPass<BloomCombinePassData>("BloomCombine Pass",
			[=](BloomCombinePassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc bloom_output_desc{};
				bloom_output_desc.width = width;
				bloom_output_desc.height = height;
				bloom_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				bloom_output_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				bloom_output_desc.initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				builder.DeclareTexture(RG_RES_NAME(BloomOutput), bloom_output_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(BloomOutput));
				data.extract = builder.ReadTexture(RG_RES_NAME(BloomExtract), ReadAccess_NonPixelShader);
				data.input  = builder.ReadTexture(final_resource, ReadAccess_NonPixelShader);
			},
			[=](BloomCombinePassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::BloomCombine));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::BloomCombine));

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = resources.GetDescriptor(data.input);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cpu_descriptor = resources.GetDescriptor(data.extract);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

				descriptor_index += 2;
				cpu_descriptor = resources.GetDescriptor(data.output);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->Dispatch((uint32)std::ceil(width / 32.0f),(uint32)std::ceil(height / 32.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

	}

	void Postprocessor::AddDepthOfFieldPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = final_resource;
		struct DepthOfFieldPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId blurred_input;
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<DepthOfFieldPassData>("DepthOfField Pass",
			[=](DepthOfFieldPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc dof_output_desc{};
				dof_output_desc.width = width;
				dof_output_desc.height = height;
				dof_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				dof_output_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				dof_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				builder.DeclareTexture(RG_RES_NAME(DepthOfFieldOutput), dof_output_desc);
				builder.WriteRenderTarget(RG_RES_NAME(DepthOfFieldOutput), ERGLoadStoreAccessOp::Discard_Preserve);
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.blurred_input = builder.ReadTexture(RG_RES_NAME(BlurredDofInput), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](DepthOfFieldPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::DOF));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::DOF));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { resources.GetDescriptor(data.input), resources.GetDescriptor(data.blurred_input), resources.GetDescriptor(data.depth) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1, 1 };
				uint32 dst_range_sizes[] = { 3 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void Postprocessor::AddGenerateBokehPasses(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct GenerateBokehPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
		};
		/*
		rg.AddPass<GenerateBokehPassData>("GenerateBokeh Pass",
			[=](GenerateBokehPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc dof_output_desc{};
				dof_output_desc.width = width;
				dof_output_desc.height = height;
				dof_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				dof_output_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				dof_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				//builder.DeclareBuffer(RG_RES_NAME(DepthOfFieldOutput), dof_output_desc);
				data.input = builder.ReadTexture(final_resource, ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](GenerateBokehPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				D3D12_RESOURCE_BARRIER prereset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
				cmd_list->ResourceBarrier(1, &prereset_barrier);
				cmd_list->CopyBufferRegion(bokeh_counter.GetNative(), 0, counter_reset_buffer->GetNative(), 0, sizeof(uint32));
				D3D12_RESOURCE_BARRIER postreset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				cmd_list->ResourceBarrier(1, &postreset_barrier);

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::BokehGenerate));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::BokehGenerate));
				cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
				cmd_list->SetComputeRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);
				cmd_list->SetComputeRootConstantBufferView(2, compute_cbuffer.View(backbuffer_index).BufferLocation);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

				D3D12_RESOURCE_BARRIER dispatch_barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
					CD3DX12_RESOURCE_BARRIER::Transition(bokeh->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				};
				cmd_list->ResourceBarrier(ARRAYSIZE(dispatch_barriers), dispatch_barriers);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index]->SRV(), depth_target->SRV() };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1 };
				uint32 dst_range_sizes[] = { 2 };
				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));

				D3D12_CPU_DESCRIPTOR_HANDLE bokeh_uav = bokeh->UAV();
				descriptor_index = descriptor_allocator->Allocate();

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), bokeh_uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->Dispatch((uint32)std::ceil(width / 32.0f), (uint32)std::ceil(height / 32.0f), 1);

				CD3DX12_RESOURCE_BARRIER precopy_barriers[] = {
						CD3DX12_RESOURCE_BARRIER::Transition(bokeh->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
						CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
						CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer->GetNative(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
						CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
				};
				cmd_list->ResourceBarrier(ARRAYSIZE(precopy_barriers), precopy_barriers);

				cmd_list->CopyBufferRegion(bokeh_indirect_draw_buffer->GetNative(), 0, bokeh_counter.GetNative(), 0, bokeh_counter.GetDesc().size);

				CD3DX12_RESOURCE_BARRIER postcopy_barriers[] =
				{
						CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer->GetNative(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
						CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				};
				cmd_list->ResourceBarrier(ARRAYSIZE(postcopy_barriers), postcopy_barriers);

			}, ERGPassType::Compute, ERGPassFlags::None);
			*/
	}

	void Postprocessor::AddDrawBokehPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		/*
		struct DepthOfFieldPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId blurred_input;
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<DepthOfFieldPassData>("DrawBokeh Pass",
			[=](DepthOfFieldPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc dof_output_desc{};
				dof_output_desc.width = width;
				dof_output_desc.height = height;
				dof_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				dof_output_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				dof_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				builder.DeclareTexture(RG_RES_NAME(DepthOfFieldOutput), dof_output_desc);
				builder.WriteRenderTarget(RG_RES_NAME(DepthOfFieldOutput), ERGLoadStoreAccessOp::Discard_Preserve);
				data.input = builder.ReadTexture(final_resource, ReadAccess_PixelShader);
				data.blurred_input = builder.ReadTexture(RG_RES_NAME(BlurredDofInput), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](DepthOfFieldPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::DOF));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::DOF));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { resources.GetDescriptor(data.input), resources.GetDescriptor(data.blurred_input), resources.GetDescriptor(data.depth) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
				uint32 src_range_sizes[] = { 1, 1, 1 };
				uint32 dst_range_sizes[] = { 3 };

				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
			*/
	}

}

