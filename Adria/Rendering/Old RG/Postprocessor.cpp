#include "Postprocessor.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "../tecs/registry.h"

namespace adria
{

	Postprocessor::Postprocessor(TextureManager& texture_manager, uint32 width, uint32 height) : texture_manager(texture_manager), width(width), height(height),
		blur_pass(width, height), copy_to_texture_pass(width, height)
	{

	}

	PostprocessData const& Postprocessor::AddPasses(RenderGraph& rg, PostprocessSettings const& settings, 
		RGTextureRef hdr_texture, RGTextureSRVRef gbuffer_normal_srv, RGTextureSRVRef depth_srv)
	{
		PostprocessData data{};
		CopyHDRPassData const& copy_data = AddCopyHDRPass(rg, hdr_texture);
		data.final_texture = copy_data.dst_texture;

		if (settings.clouds)
		{
			VolumetricCloudsPassData const& clouds_data = AddVolumetricCloudsPass(rg, depth_srv);
			BlurPassData const& blur_data = blur_pass.AddPass(rg, clouds_data.output_srv, "Volumetric Clouds");
			CopyToTexturePassData copy_texture_data = copy_to_texture_pass.AddPass(rg, copy_data.dst_rtv, blur_data.final_srv, EBlendMode::AlphaBlend);
			data.final_texture = copy_texture_data.render_target.GetResourceHandle();
		}
		if (settings.reflections == EReflections::SSR)
		{
			SSRPassData const& ssr_data = AddSSRPass(rg, data.final_texture, gbuffer_normal_srv, depth_srv);
			data.final_texture = ssr_data.output_srv.GetResourceHandle();
		}
		if (settings.fog)
		{
			FogPassData const& fog_data = AddFogPass(rg, data.final_texture, depth_srv);
			data.final_texture = fog_data.output_srv.GetResourceHandle();
		}

		return data;
	}

	void Postprocessor::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(width, height);
		copy_to_texture_pass.OnResize(width, height);
	}

	void Postprocessor::OnSceneInitialized()
	{
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\weather.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\worley.dds"));
	}

	Postprocessor::CopyHDRPassData const& Postprocessor::AddCopyHDRPass(RenderGraph& rg, RGTextureRef hdr_texture)
	{
		return rg.AddPass<CopyHDRPassData>("Copy HDR Pass",
			[=](CopyHDRPassData& data, RenderGraphBuilder& builder)
			{
				D3D12_CLEAR_VALUE rtv_clear_value{};
				rtv_clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				rtv_clear_value.Color[0] = 0.0f;
				rtv_clear_value.Color[1] = 0.0f;
				rtv_clear_value.Color[2] = 0.0f;
				rtv_clear_value.Color[3] = 0.0f;

				TextureDesc postprocess_desc{};
				postprocess_desc.clear = rtv_clear_value;
				postprocess_desc.width = width;
				postprocess_desc.height = height;
				postprocess_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				postprocess_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				postprocess_desc.initial_state = D3D12_RESOURCE_STATE_COPY_DEST;

				RGTextureRef postprocess = builder.CreateTexture("Postprocess Texture", postprocess_desc);
				data.src_texture = builder.Read(hdr_texture);
				data.dst_texture = builder.Write(postprocess);
				data.dst_rtv = builder.CreateRTV(postprocess);
			},
			[=](CopyHDRPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Texture& src_texture = resources.GetTexture(data.src_texture);
				Texture& dst_texture = resources.GetTexture(data.dst_texture);
				cmd_list->CopyResource(dst_texture.GetNative(), src_texture.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::None);
	}

	Postprocessor::VolumetricCloudsPassData const& Postprocessor::AddVolumetricCloudsPass(RenderGraph& rg, RGTextureSRVRef depth_srv)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		return rg.AddPass<VolumetricCloudsPassData>("Volumetric Clouds Pass",
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

				RGTextureRef clouds_output = builder.CreateTexture("Volumetric Clouds Output", clouds_output_desc);
				builder.Read(depth_srv.GetResourceHandle(), ReadAccess_PixelShader);
				builder.RenderTarget(builder.CreateRTV(clouds_output), ERGLoadStoreAccessOp::Discard_Preserve);
				data.output_srv = builder.CreateSRV(clouds_output);
				builder.SetViewport(width, height);
			},
			[=](VolumetricCloudsPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Clouds));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Clouds));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.weather_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(4);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { texture_manager.CpuDescriptorHandle(cloud_textures[0]),  texture_manager.CpuDescriptorHandle(cloud_textures[1]),
															 texture_manager.CpuDescriptorHandle(cloud_textures[2]), resources.GetSRV(depth_srv)};
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

	Postprocessor::SSRPassData const& Postprocessor::AddSSRPass(RenderGraph& rg,
		RGTextureRef input, RGTextureSRVRef gbuffer_normal_srv, RGTextureSRVRef depth_srv)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		return rg.AddPass<SSRPassData>("SSR Pass",
			[=](SSRPassData& data, RenderGraphBuilder& builder)
			{
				D3D12_CLEAR_VALUE rtv_clear_value{};
				rtv_clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				rtv_clear_value.Color[0] = 0.0f;
				rtv_clear_value.Color[1] = 0.0f;
				rtv_clear_value.Color[2] = 0.0f;
				rtv_clear_value.Color[3] = 0.0f;

				TextureDesc ssr_output_desc{};
				ssr_output_desc.clear = rtv_clear_value;
				ssr_output_desc.width = width;
				ssr_output_desc.height = height;
				ssr_output_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				ssr_output_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				ssr_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				RGTextureRef ssr_output = builder.CreateTexture("SSR Output", ssr_output_desc);
				builder.RenderTarget(builder.CreateRTV(ssr_output), ERGLoadStoreAccessOp::Discard_Preserve);
				builder.Read(input);
				builder.Read(gbuffer_normal_srv.GetResourceHandle());
				builder.Read(depth_srv.GetResourceHandle());
				builder.SetViewport(width, height);
				data.input_srv = builder.CreateSRV(input);
				data.output_srv = builder.CreateSRV(ssr_output);
			},
			[=](SSRPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::SSR));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::SSR));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { resources.GetSRV(gbuffer_normal_srv), resources.GetSRV(data.input_srv), resources.GetSRV(depth_srv)};
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

	Postprocessor::FogPassData const& Postprocessor::AddFogPass(RenderGraph& rg, RGTextureRef input, RGTextureSRVRef depth_srv)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		return rg.AddPass<FogPassData>("Fog Pass",
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

				RGTextureRef fog_output = builder.CreateTexture("Fog Output", fog_output_desc);
				builder.Read(depth_srv.GetResourceHandle(), ReadAccess_PixelShader);
				builder.Read(input, ReadAccess_PixelShader);
				builder.RenderTarget(builder.CreateRTV(fog_output), ERGLoadStoreAccessOp::Discard_Preserve);
				data.output_srv = builder.CreateSRV(fog_output);
				data.input_srv = builder.CreateSRV(input);
				builder.SetViewport(width, height);
			},
			[=](FogPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Fog));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Fog));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(1, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { resources.GetSRV(data.input_srv),  resources.GetSRV(depth_srv)};
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

}

