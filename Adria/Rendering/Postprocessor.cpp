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

	Postprocessor::Postprocessor(TextureManager& texture_manager, uint32 width, uint32 height) : texture_manager(texture_manager), width(width), height(height)
	{

	}

	PostprocessData const& Postprocessor::AddPasses(RenderGraph& rg, PostprocessSettings const& settings, RGTextureRef hdr_texture, RGTextureSRVRef depth_srv)
	{
		PostprocessData data{};
		CopyHDRPassData const& copy_data = AddCopyHDRPass(rg, hdr_texture);
		data.final_texture = copy_data.dst_texture;

		if (settings.clouds)
		{
			VolumetricCloudsPassData const& clouds_data = AddVolumetricCloudsPass(rg, data.final_texture, depth_srv);
			data.final_texture = clouds_data.output;
		}

		return data;
	}

	void Postprocessor::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
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
			},
			[=](CopyHDRPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Texture& src_texture = resources.GetTexture(data.src_texture);
				Texture& dst_texture = resources.GetTexture(data.dst_texture);
				cmd_list->CopyResource(dst_texture.GetNative(), src_texture.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::None);
	}

	Postprocessor::VolumetricCloudsPassData const& Postprocessor::AddVolumetricCloudsPass(RenderGraph& rg, RGTextureRef input, RGTextureSRVRef depth_srv)
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
				clouds_output_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				clouds_output_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

				RGTextureRef clouds_output = builder.CreateTexture("Volumetric Clouds Output", clouds_output_desc);
				data.output = builder.Read(input, ReadAccess_PixelShader);
				data.output = builder.RenderTarget(builder.CreateRTV(clouds_output), ERGLoadStoreAccessOp::Discard_Preserve);
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

}

