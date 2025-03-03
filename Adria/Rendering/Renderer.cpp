#include "Renderer.h"
#include "BlackboardData.h"
#include "Camera.h"
#include "Components.h"
#include "ShaderManager.h"
#include "SkyModel.h"
#include "TextureManager.h"
#include "DebugRenderer.h"

#include "Editor/GUICommand.h"
#include "Editor/Editor.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxTracyProfiler.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/ThreadPool.h"
#include "Utilities/Random.h"
#include "Utilities/ImageWrite.h"
#include "Math/Constants.h"
#include "Core/Paths.h"
#include "Core/ConsoleManager.h"
#include "entt/entity/registry.hpp"
#include "tracy/Tracy.hpp"

using namespace DirectX;

namespace adria
{
	static TAutoConsoleVariable<Int>  LightingPathType("r.LightingPath", 0, "0 - Deferred, 1 - Tiled Deferred, 2 - Clustered Deferred, 3 - Path Tracing");

	Renderer::Renderer(entt::registry& reg, GfxDevice* gfx, Uint32 width, Uint32 height) : reg(reg), gfx(gfx), resource_pool(gfx),
		accel_structure(gfx), camera(nullptr), display_width(width), display_height(height), render_width(width), render_height(height),
		backbuffer_count(gfx->GetBackbufferCount()), backbuffer_index(gfx->GetBackbufferIndex()), final_texture(nullptr),
		frame_cbuffer(gfx, backbuffer_count), gpu_driven_renderer(reg, gfx, width, height),
		gbuffer_pass(reg, gfx, width, height),
		sky_pass(reg, gfx, width, height), deferred_lighting_pass(gfx, width, height),
		tiled_deferred_lighting_pass(reg, gfx, width, height) , copy_to_texture_pass(gfx, width, height), add_textures_pass(gfx, width, height),
		postprocessor(gfx, reg, width, height), picking_pass(gfx, width, height),
		clustered_deferred_lighting_pass(reg, gfx, width, height),
		decals_pass(reg, gfx, width, height), rain_pass(reg, gfx, width, height), ocean_renderer(reg, gfx, width, height),
		shadow_renderer(reg, gfx, width, height), renderer_output_pass(gfx, width, height),
		path_tracer(gfx, width, height), ddgi(gfx, reg, width, height), restir_di(gfx, width, height), gpu_debug_printer(gfx),
		transparent_pass(reg, gfx, width, height), ray_tracing_supported(gfx->GetCapabilities().SupportsRayTracing()),
		volumetric_fog_manager(gfx, reg, width, height)
	{
		g_DebugRenderer.Initialize(gfx, width, height);
		g_GfxProfiler.Initialize(gfx);
		GfxTracyProfiler::Initialize(gfx);
		CreateDisplaySizeDependentResources();
		CreateRenderSizeDependentResources();
		RegisterEventListeners();
		screenshot_fence.Create(gfx, "Screenshot Fence");
	}

	Renderer::~Renderer()
	{
		GfxTracyProfiler::Destroy();
		g_GfxProfiler.Destroy();
		gfx->WaitForGPU();
		reg.clear();
		gfxcommon::Destroy();
	}

	void Renderer::SetRendererOutput(RendererOutput type)
	{
		renderer_output = type;
		gbuffer_pass.OnRendererOutputChanged(type);
		gpu_driven_renderer.OnRendererOutputChanged(type);
	}

	void Renderer::SetLightingPath(LightingPath path)
	{
		lighting_path = path;
		LightingPathType->Set((Int)path);
	}

	void Renderer::SetViewportData(ViewportData const& vp)
	{
		viewport_data = vp;
	}
	void Renderer::NewFrame(Camera const* _camera)
	{
		ADRIA_ASSERT(_camera);
		camera = _camera;
		backbuffer_index = gfx->GetBackbufferIndex();
		g_GfxProfiler.NewFrame();
		GfxTracyProfiler::NewFrame();
	}
	void Renderer::Update(Float dt)
	{
		shadow_renderer.SetupShadows(camera);
		UpdateSceneBuffers();
		UpdateFrameConstants(dt);
		CameraFrustumCulling();
	}
	void Renderer::Render()
	{
		ZoneScopedN("Renderer::Render");
		RenderGraph render_graph(resource_pool);
		RenderImpl(render_graph);
		render_graph.Compile();
		render_graph.Execute();
		g_Editor.EndFrame();
	}

	void Renderer::OnResize(Uint32 w, Uint32 h)
	{
		if (display_width != w || display_height != h)
		{
			display_width = w; display_height = h;
			CreateDisplaySizeDependentResources();
			postprocessor.OnResize(w, h);
			g_DebugRenderer.OnResize(w, h);
			path_tracer.OnResize(w, h);
			renderer_output_pass.OnResize(w, h);
		}
	}
	void Renderer::OnRenderResolutionChanged(Uint32 w, Uint32 h)
	{
		if (render_width != w || render_height != h)
		{
			render_width = w, render_height = h;
			CreateRenderSizeDependentResources();

			gbuffer_pass.OnResize(w, h);
			gpu_driven_renderer.OnResize(w, h);
			transparent_pass.OnResize(w, h);
			sky_pass.OnResize(w, h);
			deferred_lighting_pass.OnResize(w, h);
			tiled_deferred_lighting_pass.OnResize(w, h);
			clustered_deferred_lighting_pass.OnResize(w, h);
			copy_to_texture_pass.OnResize(w, h);
			add_textures_pass.OnResize(w, h);
			picking_pass.OnResize(w, h);
			decals_pass.OnResize(w, h);
			ocean_renderer.OnResize(w, h);
			shadow_renderer.OnResize(w, h);
			ddgi.OnResize(w, h);
			restir_di.OnResize(w, h);
			rain_pass.OnResize(w, h);
			volumetric_fog_manager.OnResize(w, h);
		}
	}

	void Renderer::OnSceneInitialized()
	{
		sheenE_texture = g_TextureManager.LoadTexture(paths::TexturesDir + "SheenE.dds");
		sky_pass.OnSceneInitialized();
		decals_pass.OnSceneInitialized();
		rain_pass.OnSceneInitialized();
		postprocessor.OnSceneInitialized();
		ocean_renderer.OnSceneInitialized();
		ddgi.OnSceneInitialized();
		volumetric_fog_manager.OnSceneInitialized();
		CreateAS();

		gfxcommon::Initialize(gfx);
		g_TextureManager.OnSceneInitialized();
	}
	void Renderer::OnRightMouseClicked(Int32 x, Int32 y)
	{
		update_picking_data = true;
	}
	void Renderer::OnTakeScreenshot(Char const* filename)
	{
		screenshot_name = filename;
		if (screenshot_name.empty())
		{
			static Uint32 screenshot_index = 0;
			screenshot_name = "adria_screenshot";
			screenshot_name += std::to_string(screenshot_index++);
		}
		take_screenshot = true;
	}

	void Renderer::OnLightChanged()
	{
		path_tracer.Reset();
	}

	void Renderer::RegisterEventListeners()
	{
		postprocessor.AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate::CreateMember(&Renderer::OnRenderResolutionChanged, *this));
		shadow_renderer.GetShadowTextureRenderedEvent().AddMember(&DeferredLightingPass::OnShadowTextureRendered, deferred_lighting_pass);
		shadow_renderer.GetShadowTextureRenderedEvent().AddMember(&VolumetricFogManager::OnShadowTextureRendered, volumetric_fog_manager);

		rain_pass.GetRainEvent().AddMember(&PostProcessor::OnRainEvent, postprocessor);
		rain_pass.GetRainEvent().AddMember(&GPUDrivenGBufferPass::OnRainEvent, gpu_driven_renderer);
		rain_pass.GetRainEvent().AddMember(&GBufferPass::OnRainEvent, gbuffer_pass);

		transparent_pass.GetTransparentChangedEvent().AddMember(&GPUDrivenGBufferPass::OnTransparentChanged, gpu_driven_renderer);
		transparent_pass.GetTransparentChangedEvent().AddMember(&GBufferPass::OnTransparentChanged, gbuffer_pass);

		LightingPathType->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) { lighting_path = static_cast<LightingPath>(cvar->GetInt()); }));
	}

	void Renderer::CreateDisplaySizeDependentResources()
	{
		GfxTextureDesc final_texture_desc{};
		final_texture_desc.width = display_width;
		final_texture_desc.height = display_height;
		final_texture_desc.format = GfxFormat::R8G8B8A8_UNORM;
		final_texture_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource | GfxBindFlag::RenderTarget;
		final_texture_desc.initial_state = GfxResourceState::ComputeUAV;
		final_texture = gfx->CreateTexture(final_texture_desc);
	}

	void Renderer::CreateRenderSizeDependentResources()
	{
		GfxTextureDesc overdraw_texture_desc{};
		overdraw_texture_desc.width = render_width;
		overdraw_texture_desc.height = render_height;
		overdraw_texture_desc.format = GfxFormat::R32_UINT;
		overdraw_texture_desc.bind_flags = GfxBindFlag::UnorderedAccess;
		overdraw_texture_desc.initial_state = GfxResourceState::ComputeUAV;
		overdraw_texture = gfx->CreateTexture(overdraw_texture_desc);
		overdraw_texture_uav = gfx->CreateTextureUAV(overdraw_texture.get());
	}

	void Renderer::CreateAS()
	{
		if (!ray_tracing_supported) return;
		if (reg.view<RayTracing>().size() == 0) return;

		accel_structure.Clear();
		auto ray_tracing_view = reg.view<Mesh, RayTracing>();
		for (auto entity : ray_tracing_view)
		{
			Mesh const& mesh = ray_tracing_view.get<Mesh>(entity);
			accel_structure.AddInstance(mesh);
		}
		accel_structure.Build();
	}

	void Renderer::UpdateSceneBuffers()
	{
		for (auto e : reg.view<Batch>()) reg.destroy(e);
		reg.clear<Batch>();

		std::vector<LightGPU> hlsl_lights{};
		Uint32 light_index = 0;
		Matrix light_transform = lighting_path == LightingPath::PathTracing ? Matrix::Identity : camera->View();
		for (auto light_entity : reg.view<Light>())
		{
			Light& light = reg.get<Light>(light_entity);
			light.light_index = light_index;
			++light_index;

			LightGPU& hlsl_light = hlsl_lights.emplace_back();
			hlsl_light.color = light.color * light.intensity;
			hlsl_light.position = Vector4::Transform(light.position, light_transform);
			hlsl_light.direction = Vector4::Transform(light.direction, light_transform);
			hlsl_light.range = light.range;
			hlsl_light.type = static_cast<Int32>(light.type);
			hlsl_light.inner_cosine = light.inner_cosine;
			hlsl_light.outer_cosine = light.outer_cosine;
			hlsl_light.volumetric = light.volumetric;
			hlsl_light.volumetric_strength = light.volumetric_strength;
			hlsl_light.active = light.active;
			hlsl_light.shadow_matrix_index = light.casts_shadows ? light.shadow_matrix_index : -1;
			hlsl_light.shadow_texture_index = light.casts_shadows ? light.shadow_texture_index : -1;
			hlsl_light.shadow_mask_index = light.ray_traced_shadows ? light.shadow_mask_index : -1;
			hlsl_light.use_cascades = light.use_cascades;
		}

		std::vector<MeshGPU> meshes;
		std::vector<InstanceGPU> instances;
		std::vector<MaterialGPU> materials;
		Uint32 instanceID = 0;

		for (auto mesh_entity : reg.view<Mesh>())
		{
			Mesh& mesh = reg.get<Mesh>(mesh_entity);

			GfxBuffer* mesh_buffer = g_GeometryBufferCache.GetGeometryBuffer(mesh.geometry_buffer_handle);
			GfxDescriptor mesh_buffer_srv = g_GeometryBufferCache.GetGeometryBufferSRV(mesh.geometry_buffer_handle);
			GfxDescriptor mesh_buffer_online_srv = gfx->AllocateDescriptorsGPU();
			gfx->CopyDescriptors(1, mesh_buffer_online_srv, mesh_buffer_srv);

			for (auto const& instance : mesh.instances)
			{
				SubMeshGPU& submesh = mesh.submeshes[instance.submesh_index];
				Material& material = mesh.materials[submesh.material_index];

				submesh.buffer_address = mesh_buffer->GetGpuAddress();

				entt::entity batch_entity = reg.create();
				if (material.alpha_mode == MaterialAlphaMode::Blend)
				{
					reg.emplace<Transparent>(batch_entity);
				}
				Batch& batch = reg.emplace<Batch>(batch_entity);
				batch.instance_id = instanceID;
				batch.alpha_mode = material.alpha_mode;
				batch.shading_extension = material.shading_extension;
				batch.submesh = &submesh;
				batch.world_transform = instance.world_transform;
				submesh.bounding_box.Transform(batch.bounding_box, batch.world_transform);
				

				InstanceGPU& instance_gpu = instances.emplace_back();
				instance_gpu.instance_id = instanceID;
				instance_gpu.material_idx = static_cast<Uint32>(materials.size() + submesh.material_index);
				instance_gpu.mesh_index = static_cast<Uint32>(meshes.size() + instance.submesh_index);
				instance_gpu.world_matrix = instance.world_transform;
				instance_gpu.inverse_world_matrix = XMMatrixInverse(nullptr, instance.world_transform);
				instance_gpu.bb_origin = submesh.bounding_box.Center;
				instance_gpu.bb_extents = submesh.bounding_box.Extents;

				++instanceID;
			}
			for (auto const& submesh : mesh.submeshes)
			{
				MeshGPU& mesh_gpu = meshes.emplace_back();
				mesh_gpu.buffer_idx = mesh_buffer_online_srv.GetIndex();
				mesh_gpu.indices_offset = submesh.indices_offset;
				mesh_gpu.positions_offset = submesh.positions_offset;
				mesh_gpu.normals_offset = submesh.normals_offset;
				mesh_gpu.tangents_offset = submesh.tangents_offset;
				mesh_gpu.uvs_offset = submesh.uvs_offset;

				mesh_gpu.meshlet_offset = submesh.meshlet_offset;
				mesh_gpu.meshlet_vertices_offset = submesh.meshlet_vertices_offset;
				mesh_gpu.meshlet_triangles_offset = submesh.meshlet_triangles_offset;
				mesh_gpu.meshlet_count = submesh.meshlet_count;
			}

			for (auto const& material : mesh.materials)
			{
				MaterialGPU& material_gpu = materials.emplace_back();
				material_gpu.shading_extension = (Uint32)material.shading_extension;
				material_gpu.albedo_color = Vector3(material.albedo_color);
				material_gpu.albedo_idx = (Uint32)material.albedo_texture;
				material_gpu.roughness_metallic_idx = (Uint32)material.metallic_roughness_texture;
				material_gpu.metallic_factor = material.metallic_factor;
				material_gpu.roughness_factor = material.roughness_factor;

				material_gpu.normal_idx = (Uint32)material.normal_texture;
				material_gpu.emissive_idx = (Uint32)material.emissive_texture;
				material_gpu.emissive_factor = material.emissive_factor;
				material_gpu.alpha_cutoff = material.alpha_cutoff;
				material_gpu.alpha_blended = material.alpha_mode == MaterialAlphaMode::Blend;

				material_gpu.anisotropy_idx = (Int32)material.anisotropy_texture;
				material_gpu.anisotropy_strength = material.anisotropy_strength;
				material_gpu.anisotropy_rotation = material.anisotropy_rotation;

				material_gpu.clear_coat_idx = (Uint32)material.clear_coat_texture;
				material_gpu.clear_coat_roughness_idx = (Uint32)material.clear_coat_roughness_texture;
				material_gpu.clear_coat_normal_idx = (Uint32)material.clear_coat_normal_texture;
				material_gpu.clear_coat = material.clear_coat;
				material_gpu.clear_coat_roughness = material.clear_coat_roughness;

				material_gpu.sheen_color = Vector3(material.sheen_color);
				material_gpu.sheen_color_idx = (Uint32)material.sheen_color_texture;
				material_gpu.sheen_roughness = material.sheen_roughness;
				material_gpu.sheen_roughness_idx = (Uint32)material.sheen_roughness_texture;
			}
		}

		auto CopyBuffer = [&]<typename T>(std::vector<T> const& data, SceneBuffer& scene_buffer)
		{
			if (data.empty()) return;
			if (!scene_buffer.buffer || scene_buffer.buffer->GetCount() < data.size())
			{
				scene_buffer.buffer = gfx->CreateBuffer(StructuredBufferDesc<T>(data.size(), false, true));
				scene_buffer.buffer_srv = gfx->CreateBufferSRV(scene_buffer.buffer.get());
			}
			scene_buffer.buffer->Update(data.data(), data.size() * sizeof(T));
			scene_buffer.buffer_srv_gpu = gfx->AllocateDescriptorsGPU();
			gfx->CopyDescriptors(1, scene_buffer.buffer_srv_gpu, scene_buffer.buffer_srv);
		};
		CopyBuffer(hlsl_lights, scene_buffers[SceneBuffer_Light]);
		CopyBuffer(meshes, scene_buffers[SceneBuffer_Mesh]);
		CopyBuffer(instances, scene_buffers[SceneBuffer_Instance]);
		CopyBuffer(materials, scene_buffers[SceneBuffer_Material]);
	}

	void Renderer::UpdateFrameConstants(Float dt)
	{
		static Float total_time = 0.0f;
		total_time += dt;
		rain_pass.Update(dt);

		camera_jitter = Vector2(0.0f, 0.0f);
		if (postprocessor.NeedsJitter()) camera_jitter = camera->Jitter(gfx->GetFrameIndex());
		if (camera->IsChanged()) path_tracer.Reset();

		frame_cbuf_data.camera_near = camera->Near();
		frame_cbuf_data.camera_far = camera->Far();
		frame_cbuf_data.camera_position = camera->Position();
		frame_cbuf_data.camera_forward = camera->Forward();
		frame_cbuf_data.view = camera->View();
		frame_cbuf_data.projection = camera->Proj();
		frame_cbuf_data.view_projection = camera->ViewProj();
		frame_cbuf_data.inverse_view = camera->View().Invert();
		frame_cbuf_data.inverse_projection = camera->Proj().Invert();
		frame_cbuf_data.inverse_view_projection = camera->ViewProj().Invert();
		frame_cbuf_data.reprojection = frame_cbuf_data.inverse_view_projection * frame_cbuf_data.prev_view_projection;
		frame_cbuf_data.camera_jitter_x = (camera_jitter.x * 2.0f) / render_width;
		frame_cbuf_data.camera_jitter_y = (camera_jitter.y * 2.0f) / render_height;
		frame_cbuf_data.display_resolution_x = (Float)display_width;
		frame_cbuf_data.display_resolution_y = (Float)display_height;
		frame_cbuf_data.render_resolution_x = (Float)render_width;
		frame_cbuf_data.render_resolution_y = (Float)render_height;
		frame_cbuf_data.delta_time = dt;
		frame_cbuf_data.total_time = total_time;
		frame_cbuf_data.frame_count = gfx->GetFrameIndex();
		frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
		frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;
		frame_cbuf_data.env_map_idx = sky_pass.GetSkyIndex();
		frame_cbuf_data.meshes_idx = (Int32)scene_buffers[SceneBuffer_Mesh].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.materials_idx = (Int32)scene_buffers[SceneBuffer_Material].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.instances_idx = (Int32)scene_buffers[SceneBuffer_Instance].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.lights_idx = (Int32)scene_buffers[SceneBuffer_Light].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.light_count = (Int32)scene_buffers[SceneBuffer_Light].buffer->GetCount();
		shadow_renderer.FillFrameCBuffer(frame_cbuf_data);
		frame_cbuf_data.ddgi_volumes_idx = ddgi.IsEnabled() ? ddgi.GetDDGIVolumeIndex() : -1;
		frame_cbuf_data.printf_buffer_idx = gpu_debug_printer.GetPrintfBufferIndex();
		frame_cbuf_data.rain_splash_diffuse_idx = rain_pass.GetRainSplashDiffuseIndex();
		frame_cbuf_data.rain_splash_bump_idx = rain_pass.GetRainSplashBumpIndex();
		frame_cbuf_data.rain_blocker_map_idx = rain_pass.GetRainBlockerMapIndex();
		frame_cbuf_data.rain_view_projection = rain_pass.GetRainViewProjection();
		frame_cbuf_data.sheenE_idx = (Int32)sheenE_texture;
		frame_cbuf_data.rain_total_time = rain_pass.GetRainTotalTime();
		if (ray_tracing_supported && reg.view<RayTracing>().size())
		{
			frame_cbuf_data.accel_struct_idx = accel_structure.GetTLASIndex();
		}
		if (renderer_output == RendererOutput::TriangleOverdraw)
		{
			overdraw_texture_uav_gpu = gfx->AllocateDescriptorsGPU();
			gfx->CopyDescriptors(1, overdraw_texture_uav_gpu, overdraw_texture_uav);
			frame_cbuf_data.triangle_overdraw_idx = overdraw_texture_uav_gpu.GetIndex();
		}

		auto lights = reg.view<Light>();
		for (auto light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (light_data.type == LightType::Directional && light_data.active)
			{
				frame_cbuf_data.sun_direction = -light_data.direction;
				frame_cbuf_data.sun_color = light_data.color * light_data.intensity;
				sun_direction = Vector3(light_data.direction);
				break;
			}
		}

		frame_cbuf_data.ambient_color = Vector4(ambient_color[0], ambient_color[1], ambient_color[2], 1.0f);
		frame_cbuf_data.wind_params = Vector4(wind_dir[0], wind_dir[1], wind_dir[2], wind_speed);
		frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);

		frame_cbuf_data.prev_view_projection = camera->ViewProj();
		frame_cbuf_data.prev_view = camera->View();
		frame_cbuf_data.prev_projection = camera->Proj();
	}
	void Renderer::CameraFrustumCulling()
	{
		BoundingFrustum camera_frustum = camera->Frustum();
		auto batch_view = reg.view<Batch>();
		auto light_view = reg.view<Light>();
		for (auto e : batch_view)
		{
			Batch& batch = batch_view.get<Batch>(e);
			auto& aabb = batch.bounding_box;
			batch.camera_visibility = camera_frustum.Intersects(aabb);
		}
	}

	void Renderer::RenderImpl(RenderGraph& render_graph)
	{
		ZoneScopedN("Renderer::RenderImpl");
		RG_SCOPE(render_graph, "Frame");
		RGBlackboard& rg_blackboard = render_graph.GetBlackboard();
		FrameBlackboardData frame_data{};
		{
			Vector3 cam_pos = camera->Position();
			frame_data.camera_position[0] = cam_pos.x;
			frame_data.camera_position[1] = cam_pos.y;
			frame_data.camera_position[2] = cam_pos.z;
			frame_data.camera_position[3] = 1.0f;
			frame_data.camera_view = camera->View();
			frame_data.camera_proj = camera->Proj();
			frame_data.camera_viewproj = camera->ViewProj();
			frame_data.camera_fov = camera->Fov();
			frame_data.camera_aspect_ratio = camera->AspectRatio();
			frame_data.camera_near = camera->Near();
			frame_data.camera_far = camera->Far();
			frame_data.camera_jitter_x = camera_jitter.x;
			frame_data.camera_jitter_y = camera_jitter.y;
			frame_data.delta_time = frame_cbuf_data.delta_time;
			frame_data.frame_cbuffer_address = frame_cbuffer.GetGpuAddress(backbuffer_index);
		}
		rg_blackboard.Add<FrameBlackboardData>(std::move(frame_data));
		render_graph.ImportTexture(RG_NAME(Backbuffer), gfx->GetBackbuffer());
		render_graph.ImportTexture(RG_NAME(FinalTexture), final_texture.get());
		postprocessor.ImportHistoryResources(render_graph);

		gpu_debug_printer.AddClearPass(render_graph);
		if (lighting_path == LightingPath::PathTracing) Render_PathTracing(render_graph);
		else Render_Deferred(render_graph);
		if (take_screenshot) TakeScreenshot(render_graph);
		gpu_debug_printer.AddPrintPass(render_graph);
		if (!g_Editor.IsActive())
		{
			CopyToBackbuffer(render_graph);
		}
		else 
		{
			g_Editor.AddRenderPass(render_graph);
		}

		GUI();
	}
	void Renderer::Render_Deferred(RenderGraph& render_graph)
	{
		ZoneScopedN("Renderer::Render_Deferred");
		if (update_picking_data)
		{
			picking_data = picking_pass.GetPickingData();
			update_picking_data = false;
		}
		if(renderer_output == RendererOutput::TriangleOverdraw)
		{
			ClearTriangleOverdrawTexture(render_graph);
		}
		if (rain_pass.IsEnabled()) rain_pass.AddBlockerPass(render_graph);
		
		if (gpu_driven_renderer.IsEnabled()) gpu_driven_renderer.AddPasses(render_graph);
		else gbuffer_pass.AddPass(render_graph);

		if(ddgi.IsEnabled()) ddgi.AddPasses(render_graph);
		decals_pass.AddPass(render_graph);
		postprocessor.AddAmbientOcclusionPass(render_graph);
		{
			RG_SCOPE(render_graph, "Shadows");
			shadow_renderer.AddShadowMapPasses(render_graph);
			shadow_renderer.AddRayTracingShadowPasses(render_graph);
		}

		if (renderer_output == RendererOutput::Final)
		{
			{
				RG_SCOPE(render_graph, "Lighting");
				switch (lighting_path)
				{
				case LightingPath::Deferred:			deferred_lighting_pass.AddPass(render_graph); break;
				case LightingPath::TiledDeferred:		tiled_deferred_lighting_pass.AddPass(render_graph); break;
				case LightingPath::ClusteredDeferred:	clustered_deferred_lighting_pass.AddPass(render_graph, true); break;
				}
				volumetric_fog_manager.AddPass(render_graph);
			}

			if (ddgi.IsEnabled() && ddgi.Visualize())
			{
				ddgi.AddVisualizePass(render_graph);
			}
			{
				RG_SCOPE(render_graph, "Forward");
				ocean_renderer.AddPasses(render_graph);
				sky_pass.AddPasses(render_graph, sun_direction);
				transparent_pass.AddPass(render_graph);
				picking_pass.AddPass(render_graph);
				if (rain_pass.IsEnabled()) rain_pass.AddPass(render_graph);
			}
			postprocessor.AddPasses(render_graph);
			g_DebugRenderer.Render(render_graph);
		}
		else
		{
			if (renderer_output == RendererOutput::MotionVectors)
			{
				postprocessor.AddMotionVectorsPass(render_graph);
			}
			renderer_output_pass.AddPass(render_graph, renderer_output);
		}
	}
	void Renderer::Render_PathTracing(RenderGraph& render_graph)
	{
		ZoneScopedN("Renderer::Render_PathTracing");
		path_tracer.AddPass(render_graph);
		postprocessor.AddTonemapPass(render_graph, path_tracer.GetFinalOutput());
	}

	void Renderer::GUI()
	{
		if (gpu_driven_renderer.IsSupported())
		{
			gpu_driven_renderer.GUI();
		}
		if (ddgi.IsSupported())
		{
			ddgi.GUI();
		}
		if (lighting_path == LightingPath::TiledDeferred)
		{
			tiled_deferred_lighting_pass.GUI();
		}
		else if (lighting_path == LightingPath::PathTracing)
		{
			path_tracer.GUI();
		}
		shadow_renderer.GUI();
		ocean_renderer.GUI();
		sky_pass.GUI();
		rain_pass.GUI();
		transparent_pass.GUI();
		volumetric_fog_manager.GUI();
		QueueGUI([&]()
			{
				if (ImGui::TreeNode("Weather Settings"))
				{
					if (ImGui::TreeNode("Sun Settings"))
					{
						auto lights = reg.view<Light, Transform>();
						Light* sun_light = nullptr;
						Transform* sun_transform = nullptr;
						for (entt::entity light : lights)
						{
							Light& light_data = lights.get<Light>(light);
							if (light_data.type == LightType::Directional && light_data.active)
							{
								sun_light = &light_data;
								sun_transform = &lights.get<Transform>(light);
								break;
							}
						}
						if (sun_light)
						{
							static Float sun_elevation = 75.0f;
							static Float sun_azimuth = 260.0f;
							ConvertDirectionToAzimuthAndElevation(-sun_light->direction, sun_elevation, sun_azimuth);

							Bool changed = false;
							changed |= ImGui::ColorEdit3("Sun Color", &sun_light->color.x);
							changed |= ImGui::SliderFloat("Sun Energy", &sun_light->intensity, 0.0f, 50.0f);
							changed |= ImGui::SliderFloat("Sun Elevation", &sun_elevation, -90.0f, 90.0f);
							changed |= ImGui::SliderFloat("Sun Azimuth", &sun_azimuth, 0.0f, 360.0f);

							if (changed)
							{
								path_tracer.Reset();
							}
							sun_light->direction = ConvertElevationAndAzimuthToDirection(sun_elevation, sun_azimuth);
							sun_light->position = 1e3 * sun_light->direction;
							sun_light->direction = -sun_light->direction;
							sun_transform->current_transform = XMMatrixTranslationFromVector(sun_light->position);
						}
						ImGui::TreePop();
					}
					ImGui::ColorEdit3("Ambient Color", ambient_color);
					ImGui::SliderFloat3("Wind Direction", wind_dir, -1.0f, 1.0f);
					ImGui::SliderFloat("Wind Speed", &wind_speed, 0.0f, 32.0f);
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer);
		renderer_output_pass.GUI();
		postprocessor.GUI();
	}

	void Renderer::ClearTriangleOverdrawTexture(RenderGraph& rg)
	{
		rg.AddPass<void>("Clear Triangle Overdraw Texture Pass",
			[=](RenderGraphBuilder& builder)
			{},
			[&](RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				Uint32 clear[] = { 0,0,0,0 };
				cmd_list->ClearUAV(*overdraw_texture, overdraw_texture_uav_gpu, overdraw_texture_uav, clear);
			}, RGPassType::Compute, RGPassFlags::ForceNoCull);
	}

	void Renderer::CopyToBackbuffer(RenderGraph& rg)
	{
		struct CopyToBackbufferPassData
		{
			RGTextureCopyDstId dst;
			RGTextureCopySrcId src;
		};

		rg.AddPass<CopyToBackbufferPassData>("Copy to Backbuffer Pass",
			[=](CopyToBackbufferPassData& data, RenderGraphBuilder& builder)
			{
				data.dst = builder.WriteCopyDstTexture(RG_NAME(Backbuffer));
				data.src = builder.ReadCopySrcTexture(RG_NAME(FinalTexture));
			},
			[=](CopyToBackbufferPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture const& src_texture = ctx.GetCopySrcTexture(data.src);
				GfxTexture& dst_texture = ctx.GetCopyDstTexture(data.dst);
				cmd_list->CopyTexture(dst_texture, src_texture);
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);
	}

	void Renderer::TakeScreenshot(RenderGraph& rg)
	{
		ADRIA_ASSERT(take_screenshot);

		std::string absolute_screenshot_path = paths::ScreenshotsDir + screenshot_name + ".png";
		ADRIA_LOG(INFO, "Taking screenshot: %s.png...", screenshot_name.c_str());
		if (!screenshot_buffer)
		{	
			GfxBufferDesc screenshot_desc{};
			screenshot_desc.size = gfx->GetLinearBufferSize(final_texture.get()); 
			screenshot_desc.resource_usage = GfxResourceUsage::Readback;
			screenshot_buffer = gfx->CreateBuffer(screenshot_desc);
		}
		rg.ImportBuffer(RG_NAME(ScreenshotBuffer), screenshot_buffer.get());

		struct ScreenshotPassData
		{
			RGBufferCopyDstId  dst;
			RGTextureCopySrcId src;
		};
		rg.AddPass<ScreenshotPassData>("Screenshot Pass",
			[=](ScreenshotPassData& data, RenderGraphBuilder& builder)
			{
				data.dst = builder.WriteCopyDstBuffer(RG_NAME(ScreenshotBuffer));
				data.src = builder.ReadCopySrcTexture(RG_NAME(FinalTexture));
			},
			[=](ScreenshotPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture const& src_texture = ctx.GetCopySrcTexture(data.src);
				GfxBuffer& dst_buffer = ctx.GetCopyDstBuffer(data.dst);
				cmd_list->CopyTextureToBuffer(dst_buffer, 0, src_texture, 0, 0);
				cmd_list->Signal(screenshot_fence, screenshot_fence_value);
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);

		g_ThreadPool.Submit([this](std::string_view name) 
			{
			screenshot_fence.Wait(screenshot_fence_value);
			WriteImageToFile(FileType::PNG, name.data(), display_width, display_height, screenshot_buffer->GetMappedData(), display_width * 4);
			screenshot_buffer.reset();
			ADRIA_LOG(INFO, "Screenshot %s.png saved to screenshots folder!", screenshot_name.c_str());
			screenshot_fence_value++;
			}, absolute_screenshot_path);

		take_screenshot = false;
	}

}

