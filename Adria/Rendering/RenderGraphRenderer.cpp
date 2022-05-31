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
		backbuffer_count(gfx->BackbufferCount()), backbuffer_index(gfx->BackbufferIndex()), //final_texture(nullptr),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count), postprocess_cbuffer(gfx->GetDevice(), backbuffer_count),
		gbuffer_pass(reg, gpu_profiler, width, height), ambient_pass(width, height), tonemap_pass(width, height)
	{
		RootSigPSOManager::Initialize(gfx->GetDevice());
		CreateNullHeap();
		CreateSizeDependentResources();
	}

	RenderGraphRenderer::~RenderGraphRenderer()
	{
		gfx->WaitForGPU();
		RootSigPSOManager::Destroy();
		reg.clear();
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
		CameraFrustumCulling();
	}
	void RenderGraphRenderer::Render(RendererSettings const& _settings)
	{
		settings = _settings;
		RenderGraph render_graph(resource_pool);
		RGBlackboard& rg_blackboard = render_graph.GetBlackboard();

		RendererGlobalData global_data{};
		{
			global_data.frame_cbuffer_address = frame_cbuffer.BufferLocation(backbuffer_index);
			global_data.postprocess_cbuffer_address = postprocess_cbuffer.BufferLocation(backbuffer_index);
			global_data.null_srv_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2D);
			global_data.null_uav_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_RWTEXTURE2D);
			global_data.null_srv_texture2darray = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2DARRAY);
			global_data.null_srv_texturecube = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURECUBE);
		}
		rg_blackboard.Add<RendererGlobalData>(std::move(global_data));

		GBufferPassData gbuffer_data = gbuffer_pass.AddPass(render_graph, profiler_settings.profile_gbuffer_pass);
		AmbientPassData ambient_data = ambient_pass.AddPass(render_graph, gbuffer_data.gbuffer_normal, gbuffer_data.gbuffer_albedo,
			gbuffer_data.gbuffer_emissive, gbuffer_data.depth_stencil);

		if (settings.gui_visible)
		{
			RGTextureRef final_texture_ref = render_graph.ImportTexture("Final Texture", final_texture.get());
			ResolveToTexture(render_graph, ambient_data.hdr, final_texture_ref);
		}
		else ResolveToBackbuffer(render_graph, ambient_data.hdr);

		render_graph.Build();
		render_graph.Execute();
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
			CreateSizeDependentResources();
			gbuffer_pass.OnResize(w, h);
			ambient_pass.OnResize(w, h);
			tonemap_pass.OnResize(w, h);
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
	void RenderGraphRenderer::CreateSizeDependentResources()
	{
		D3D12_CLEAR_VALUE rtv_clear_value{};
		rtv_clear_value.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		rtv_clear_value.Color[0] = 0.0f;
		rtv_clear_value.Color[1] = 0.0f;
		rtv_clear_value.Color[2] = 0.0f;
		rtv_clear_value.Color[3] = 0.0f;

		TextureDesc ldr_desc{};
		ldr_desc.width = width;
		ldr_desc.height = height;
		ldr_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
		ldr_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;
		ldr_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
		ldr_desc.clear = rtv_clear_value;

		final_texture = std::make_unique<Texture>(gfx, ldr_desc);
		final_texture->CreateSRV();
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

		PostprocessCBuffer postprocess_cbuf_data{};
		postprocess_cbuf_data.tone_map_exposure = settings.tonemap_exposure;
		postprocess_cbuf_data.tone_map_operator = static_cast<int>(settings.tone_map_op);
		postprocess_cbuf_data.noise_scale = XMFLOAT2((float32)width / 8, (float32)height / 8);
		postprocess_cbuf_data.ssao_power = settings.ssao_power;
		postprocess_cbuf_data.ssao_radius = settings.ssao_radius;
		for (uint32 i = 0; i < SSAO_KERNEL_SIZE; ++i) postprocess_cbuf_data.samples[i] = ssao_kernel[i];
		postprocess_cbuf_data.ssr_ray_step = settings.ssr_ray_step;
		postprocess_cbuf_data.ssr_ray_hit_threshold = settings.ssr_ray_hit_threshold;
		postprocess_cbuf_data.dof_params = XMVectorSet(settings.dof_near_blur, settings.dof_near, settings.dof_far, settings.dof_far_blur);
		postprocess_cbuf_data.velocity_buffer_scale = settings.velocity_buffer_scale;
		postprocess_cbuf_data.fog_falloff = settings.fog_falloff;
		postprocess_cbuf_data.fog_density = settings.fog_density;
		postprocess_cbuf_data.fog_type = static_cast<int32>(settings.fog_type);
		postprocess_cbuf_data.fog_start = settings.fog_start;
		postprocess_cbuf_data.fog_color = XMVectorSet(settings.fog_color[0], settings.fog_color[1], settings.fog_color[2], 1);
		postprocess_cbuf_data.hbao_r2 = settings.hbao_radius * settings.hbao_radius;
		postprocess_cbuf_data.hbao_radius_to_screen = settings.hbao_radius * 0.5f * float32(height) / (tanf(camera->Fov() * 0.5f) * 2.0f);
		postprocess_cbuf_data.hbao_power = settings.hbao_power;
		postprocess_cbuffer.Update(postprocess_cbuf_data, backbuffer_index);
	}
	void RenderGraphRenderer::CameraFrustumCulling()
	{
		BoundingFrustum camera_frustum = camera->Frustum();
		auto visibility_view = reg.view<Visibility>();
		for (auto e : visibility_view)
		{
			auto& visibility = visibility_view.get(e);
			visibility.camera_visible = camera_frustum.Intersects(visibility.aabb) || reg.has<Light>(e); //dont cull lights for now
		}
	}

	void RenderGraphRenderer::ResolveToBackbuffer(RenderGraph& rg, RGTextureRef hdr_texture)
	{
		if (HasAnyFlag(settings.anti_aliasing, AntiAliasing_FXAA))
		{
			//tone_map.AddPass(rg,...);
			//fxaa_pass.AddPass(rg,...);
		}
		else
		{
			tonemap_pass.AddPass(rg, hdr_texture);
		}

	}
	void RenderGraphRenderer::ResolveToTexture(RenderGraph& rg, RGTextureRef hdr_texture, RGTextureRef resolve_texture)
	{
		if (HasAnyFlag(settings.anti_aliasing, AntiAliasing_FXAA))
		{
			//tone_map.AddPass(rg,...);
			//fxaa_pass.AddPass(rg,...);
		}
		else
		{
			tonemap_pass.AddPass(rg, hdr_texture, resolve_texture);
		}
	}

}

