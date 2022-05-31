#include "RenderGraphRenderer.h"
#include "RendererGlobalData.h"
#include "Camera.h"
#include "Components.h"
#include "RootSigPSOManager.h"
#include "../tecs/Registry.h"
#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"
#include "../RenderGraph/RenderGraph.h"

using namespace DirectX;

namespace adria
{

	RenderGraphRenderer::RenderGraphRenderer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height) : reg(reg), gfx(gfx), resource_pool(gfx), 
		texture_manager(gfx, 1000), gpu_profiler(gfx), camera(nullptr), width(width), height(height), 
		backbuffer_count(gfx->BackbufferCount()), backbuffer_index(gfx->BackbufferIndex()),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count), 
		gbuffer_pass(reg, gpu_profiler, width, height), ambient_pass(width, height)
	{
		RootSigPSOManager::Initialize(gfx->GetDevice());
		CreateNullHeap();
	}

	void RenderGraphRenderer::NewFrame(Camera const* _camera)
	{
		ADRIA_ASSERT(_camera);
		camera = _camera;
		backbuffer_index = gfx->BackbufferIndex();
	}

	void RenderGraphRenderer::Update(float32 dt)
	{
		UpdatePersistentConstantBuffers(dt);
	}

	void RenderGraphRenderer::Render(RendererSettings const& _settings)
	{
		settings = _settings;
		RenderGraph rg_graph(resource_pool);
		RGBlackboard& rg_blackboard = rg_graph.GetBlackboard();

		RendererGlobalData global_data{};
		{
			global_data.frame_cbuffer_address = frame_cbuffer.BufferLocation(backbuffer_index);
			global_data.null_srv_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2D);
			global_data.null_uav_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_RWTEXTURE2D);
			global_data.null_srv_texture2darray = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2DARRAY);
			global_data.null_srv_texturecube = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURECUBE);
		}
		rg_blackboard.Add<RendererGlobalData>(std::move(global_data));

		GBufferPassData gbuffer_data = gbuffer_pass.AddPass(rg_graph, rg_blackboard, profiler_settings.profile_gbuffer_pass);
		AmbientPassData ambient_data = ambient_pass.AddPass(rg_graph, rg_blackboard, gbuffer_data.gbuffer_normal, gbuffer_data.gbuffer_albedo,
			gbuffer_data.gbuffer_emissive, gbuffer_data.depth_stencil);

		rg_graph.Build();
		rg_graph.Execute();

		/*
		struct TonemapPassData
		{
			RGTextureRef src;
			RGTextureSRVRef src_srv;
			RGTextureRef dst;
			RGTextureRTVRef dst_rtv;
		};

		RGTextureRef imported_texture = rg_graph.ImportTexture("LDR Target", offscreen_ldr_target.get());
		rg_graph.AddPass<TonemapPassData>("Tone Map Pass",
			[&](TonemapPassData& data, RenderGraphBuilder& builder)
			{
				data.src = builder.Read(ambient_data.hdr_rt, ReadAccess_PixelShader);
				data.src_srv = builder.CreateSRV(data.src);
				data.dst_rtv = builder.CreateRTV(imported_texture);
				data.dst = builder.RenderTarget(data.dst_rtv, ERGLoadStoreAccessOp::Discard_Preserve);
				builder.SetViewport(width, height);
			},
			[&](TonemapPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Tone Map Pass");

				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ToneMap));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ToneMap));

				cmd_list->SetGraphicsRootConstantBufferView(0, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = resources.GetSRV(data.src_srv); // postprocess_textures[!postprocess_index]->SRV();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, ERGPassType::Graphics, ERGPassFlags::ForceNoCull);

		rg_graph.Build();
		rg_graph.Execute();
		*/
	}

	void RenderGraphRenderer::SetProfilerSettings(ProfilerSettings const& _profiler_settings)
	{
		profiler_settings = _profiler_settings;
	}

	void RenderGraphRenderer::OnResize(uint32 w, uint32 h)
	{
		if (width != w || height != h)
		{
			width = w; height = h;
			gbuffer_pass.OnResize(w, h);

		}
	}

	void RenderGraphRenderer::OnSceneInitialized()
	{
		UINT tex2darray_size = (UINT)texture_manager.handle;
		gfx->ReserveOnlineDescriptors(tex2darray_size);

		ID3D12Device* device = gfx->GetDevice();
		RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		device->CopyDescriptorsSimple(tex2darray_size, descriptor_allocator->GetFirstHandle(),
			texture_manager.texture_srv_heap->GetFirstHandle(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	TextureManager& RenderGraphRenderer::GetTextureManager()
	{
		return texture_manager;
	}

	void RenderGraphRenderer::CreateNullHeap()
	{
		ID3D12Device* device = gfx->GetDevice();
		null_heap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, NULL_HEAP_SIZE);
		D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
		null_srv_desc.Texture2D.MostDetailedMip = 0;
		null_srv_desc.Texture2D.MipLevels = -1;
		null_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		null_srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2D));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURECUBE));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2DARRAY));

		D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc{};
		null_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		null_uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		device->CreateUnorderedAccessView(nullptr, nullptr, &null_uav_desc, null_heap->GetHandle(NULL_HEAP_SLOT_RWTEXTURE2D));
	}
	void RenderGraphRenderer::UpdatePersistentConstantBuffers(float32 dt)
	{
		FrameCBuffer frame_cbuf_data;
		frame_cbuf_data.global_ambient = XMVectorSet(settings.ambient_color[0], settings.ambient_color[1], settings.ambient_color[2], 1);
		frame_cbuf_data.camera_near = camera->Near();
		frame_cbuf_data.camera_far = camera->Far();
		frame_cbuf_data.camera_position = camera->Position();
		frame_cbuf_data.camera_forward = camera->Forward();
		frame_cbuf_data.view = camera->View();
		frame_cbuf_data.projection = camera->Proj();
		frame_cbuf_data.view_projection = camera->ViewProj();
		frame_cbuf_data.inverse_view = DirectX::XMMatrixInverse(nullptr, camera->View());
		frame_cbuf_data.inverse_projection = DirectX::XMMatrixInverse(nullptr, camera->Proj());
		frame_cbuf_data.inverse_view_projection = DirectX::XMMatrixInverse(nullptr, camera->ViewProj());
		frame_cbuf_data.screen_resolution_x = (float32)width;
		frame_cbuf_data.screen_resolution_y = (float32)height;
		frame_cbuf_data.mouse_normalized_coords_x = 0.0f;	//move this to some other cbuffer?
		frame_cbuf_data.mouse_normalized_coords_y = 0.0f;	//move this to some other cbuffer?

		frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);
		frame_cbuf_data.prev_view_projection = camera->ViewProj();
	}

}

