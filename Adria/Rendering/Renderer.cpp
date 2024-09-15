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
#include "Logging/Logger.h"
#include "Core/Paths.h"
#include "Core/ConsoleManager.h"
#include "entt/entity/registry.hpp"


using namespace DirectX;

namespace adria
{
	static TAutoConsoleVariable<int>  LightingPath("r.LightingPath", 0, "0 - Deferred, 1 - Tiled Deferred, 2 - Clustered Deferred, 3 - Path Tracing");
	static TAutoConsoleVariable<int>  VolumetricPath("r.VolumetricPath", 1, "0 - None, 1 - 2D Raymarching, 2 - Fog Volume");

	Renderer::Renderer(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height) : reg(reg), gfx(gfx), resource_pool(gfx),
		accel_structure(gfx), camera(nullptr), display_width(width), display_height(height), render_width(width), render_height(height),
		backbuffer_count(gfx->GetBackbufferCount()), backbuffer_index(gfx->GetBackbufferIndex()), final_texture(nullptr),
		frame_cbuffer(gfx, backbuffer_count), gpu_driven_renderer(reg, gfx, width, height),
		gbuffer_pass(reg, gfx, width, height),
		sky_pass(reg, gfx, width, height), deferred_lighting_pass(gfx, width, height), 
		volumetric_lighting_pass(gfx, width, height), volumetric_fog_pass(gfx, reg, width, height),
		tiled_deferred_lighting_pass(reg, gfx, width, height) , copy_to_texture_pass(gfx, width, height), add_textures_pass(gfx, width, height),
		postprocessor(gfx, reg, width, height), picking_pass(gfx, width, height),
		clustered_deferred_lighting_pass(reg, gfx, width, height),
		decals_pass(reg, gfx, width, height), rain_pass(reg, gfx, width, height), ocean_renderer(reg, gfx, width, height),
		shadow_renderer(reg, gfx, width, height), renderer_output_pass(gfx, width, height),
		path_tracer(gfx, width, height), ddgi(gfx, reg, width, height), gpu_debug_printer(gfx)
	{
		ray_tracing_supported = gfx->GetCapabilities().SupportsRayTracing();

		g_DebugRenderer.Initialize(gfx, width, height);
		g_GfxProfiler.Initialize(gfx);
		GfxTracyProfiler::Initialize(gfx);
		CreateSizeDependentResources();

		postprocessor.AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate::CreateMember(&Renderer::OnRenderResolutionChanged, *this));
		shadow_renderer.GetShadowTextureRenderedEvent().AddMember(&DeferredLightingPass::OnShadowTextureRendered, deferred_lighting_pass);
		shadow_renderer.GetShadowTextureRenderedEvent().AddMember(&VolumetricLightingPass::OnShadowTextureRendered, volumetric_lighting_pass);

		rain_pass.GetRainEvent().AddMember(&PostProcessor::OnRainEvent, postprocessor);
		rain_pass.GetRainEvent().AddMember(&GPUDrivenGBufferPass::OnRainEvent, gpu_driven_renderer);
		rain_pass.GetRainEvent().AddMember(&GBufferPass::OnRainEvent, gbuffer_pass);
		screenshot_fence.Create(gfx, "Screenshot Fence");

		{
			LightingPath->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) { lighting_path = static_cast<LightingPathType>(cvar->GetInt()); }));
			VolumetricPath->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) { volumetric_path = static_cast<VolumetricPathType>(cvar->GetInt()); }));
		}
	}

	Renderer::~Renderer()
	{
		GfxTracyProfiler::Destroy();
		g_GfxProfiler.Destroy();
		gfx->WaitForGPU();
		reg.clear();
		gfxcommon::Destroy();
	}

	void Renderer::SetLightingPath(LightingPathType path)
	{
		lighting_path = path;
		LightingPath->Set((int)path);
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
	void Renderer::Update(float dt)
	{
		shadow_renderer.SetupShadows(camera);
		UpdateSceneBuffers();
		UpdateFrameConstants(dt);
		CameraFrustumCulling();
	}
	void Renderer::Render()
	{
		RenderGraph render_graph(resource_pool);
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

		gpu_debug_printer.AddClearPass(render_graph);
		if (lighting_path == LightingPathType::PathTracing) Render_PathTracing(render_graph);
		else Render_Deferred(render_graph);
		if (take_screenshot) TakeScreenshot(render_graph);
		gpu_debug_printer.AddPrintPass(render_graph);

		if (!g_Editor.IsActive()) CopyToBackbuffer(render_graph);
		else g_Editor.AddRenderPass(render_graph);

		render_graph.Build();
		render_graph.Execute();

		GUI();
	}

	void Renderer::OnResize(uint32 w, uint32 h)
	{
		if (display_width != w || display_height != h)
		{
			display_width = w; display_height = h;
			CreateSizeDependentResources();
			postprocessor.OnResize(w, h);
			g_DebugRenderer.OnResize(w, h);
			path_tracer.OnResize(w, h);
			renderer_output_pass.OnResize(w, h);
		}
	}
	void Renderer::OnRenderResolutionChanged(uint32 w, uint32 h)
	{
		if (render_width != w || render_height != h)
		{
			render_width = w, render_height = h;

			gbuffer_pass.OnResize(w, h);
			gpu_driven_renderer.OnResize(w, h);
			sky_pass.OnResize(w, h);
			deferred_lighting_pass.OnResize(w, h);
			volumetric_lighting_pass.OnResize(w, h);
			volumetric_fog_pass.OnResize(w, h);
			tiled_deferred_lighting_pass.OnResize(w, h);
			clustered_deferred_lighting_pass.OnResize(w, h);
			copy_to_texture_pass.OnResize(w, h);
			add_textures_pass.OnResize(w, h);
			picking_pass.OnResize(w, h);
			decals_pass.OnResize(w, h);
			ocean_renderer.OnResize(w, h);
			shadow_renderer.OnResize(w, h);
			ddgi.OnResize(w, h);
			rain_pass.OnResize(w, h);
		}
	}

	void Renderer::OnSceneInitialized()
	{
		sky_pass.OnSceneInitialized();
		decals_pass.OnSceneInitialized();
		rain_pass.OnSceneInitialized();
		postprocessor.OnSceneInitialized();
		ocean_renderer.OnSceneInitialized();
		ddgi.OnSceneInitialized();
		volumetric_fog_pass.OnSceneInitialized();
		CreateAS();

		gfxcommon::Initialize(gfx);
		g_TextureManager.OnSceneInitialized();
	}
	void Renderer::OnRightMouseClicked(int32 x, int32 y)
	{
		update_picking_data = true;
	}
	void Renderer::OnTakeScreenshot(char const* filename)
	{
		screenshot_name = filename;
		if (screenshot_name.empty())
		{
			static uint32 screenshot_index = 0;
			screenshot_name = "adria_screenshot";
			screenshot_name += std::to_string(screenshot_index++);
		}
		take_screenshot = true;
	}

	void Renderer::CreateSizeDependentResources()
	{
		GfxTextureDesc ldr_desc{};
		ldr_desc.width = display_width;
		ldr_desc.height = display_height;
		ldr_desc.format = GfxFormat::R8G8B8A8_UNORM;
		ldr_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource | GfxBindFlag::RenderTarget;
		ldr_desc.initial_state = GfxResourceState::ComputeUAV;
		final_texture = gfx->CreateTexture(ldr_desc);
	}
	void Renderer::CreateAS()
	{
		if (!ray_tracing_supported) return;
		if (reg.view<RayTracing>().size() == 0) return;

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
		volumetric_lights = 0;
		for (auto e : reg.view<Batch>()) reg.destroy(e);
		reg.clear<Batch>();

		std::vector<LightGPU> hlsl_lights{};
		uint32 light_index = 0;
		Matrix light_transform = lighting_path == LightingPathType::PathTracing ? Matrix::Identity : camera->View();
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
			hlsl_light.type = static_cast<int32>(light.type);
			hlsl_light.inner_cosine = light.inner_cosine;
			hlsl_light.outer_cosine = light.outer_cosine;
			hlsl_light.volumetric = light.volumetric;
			hlsl_light.volumetric_strength = light.volumetric_strength;
			hlsl_light.active = light.active;
			hlsl_light.shadow_matrix_index = light.casts_shadows ? light.shadow_matrix_index : -1;
			hlsl_light.shadow_texture_index = light.casts_shadows ? light.shadow_texture_index : -1;
			hlsl_light.shadow_mask_index = light.ray_traced_shadows ? light.shadow_mask_index : -1;
			hlsl_light.use_cascades = light.use_cascades;
			if (light.volumetric) ++volumetric_lights;
		}

		std::vector<MeshGPU> meshes;
		std::vector<InstanceGPU> instances;
		std::vector<MaterialGPU> materials;
		uint32 instanceID = 0;

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
				Batch& batch = reg.emplace<Batch>(batch_entity);
				batch.instance_id = instanceID;
				batch.alpha_mode = material.alpha_mode;
				batch.submesh = &submesh;
				batch.world_transform = instance.world_transform;
				submesh.bounding_box.Transform(batch.bounding_box, batch.world_transform);

				InstanceGPU& instance_hlsl = instances.emplace_back();
				instance_hlsl.instance_id = instanceID;
				instance_hlsl.material_idx = static_cast<uint32>(materials.size() + submesh.material_index);
				instance_hlsl.mesh_index = static_cast<uint32>(meshes.size() + instance.submesh_index);
				instance_hlsl.world_matrix = instance.world_transform;
				instance_hlsl.inverse_world_matrix = XMMatrixInverse(nullptr, instance.world_transform);
				instance_hlsl.bb_origin = submesh.bounding_box.Center;
				instance_hlsl.bb_extents = submesh.bounding_box.Extents;

				++instanceID;
			}
			for (auto const& submesh : mesh.submeshes)
			{
				MeshGPU& mesh_hlsl = meshes.emplace_back();
				mesh_hlsl.buffer_idx = mesh_buffer_online_srv.GetIndex();
				mesh_hlsl.indices_offset = submesh.indices_offset;
				mesh_hlsl.positions_offset = submesh.positions_offset;
				mesh_hlsl.normals_offset = submesh.normals_offset;
				mesh_hlsl.tangents_offset = submesh.tangents_offset;
				mesh_hlsl.uvs_offset = submesh.uvs_offset;

				mesh_hlsl.meshlet_offset = submesh.meshlet_offset;
				mesh_hlsl.meshlet_vertices_offset = submesh.meshlet_vertices_offset;
				mesh_hlsl.meshlet_triangles_offset = submesh.meshlet_triangles_offset;
				mesh_hlsl.meshlet_count = submesh.meshlet_count;
			}

			for (auto const& material : mesh.materials)
			{
				MaterialGPU& material_hlsl = materials.emplace_back();
				material_hlsl.diffuse_idx = (uint32)material.albedo_texture;
				material_hlsl.normal_idx = (uint32)material.normal_texture;
				material_hlsl.roughness_metallic_idx = (uint32)material.metallic_roughness_texture;
				material_hlsl.emissive_idx = (uint32)material.emissive_texture;
				material_hlsl.base_color_factor = Vector3(material.base_color);
				material_hlsl.emissive_factor = material.emissive_factor;
				material_hlsl.metallic_factor = material.metallic_factor;
				material_hlsl.roughness_factor = material.roughness_factor;
				material_hlsl.alpha_cutoff = material.alpha_cutoff;
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

		uint64 count = reg.size();
	}

	void Renderer::UpdateFrameConstants(float dt)
	{
		static float total_time = 0.0f;
		total_time += dt;
		rain_pass.Update(dt);

		camera_jitter = Vector2(0.0f, 0.0f);
		if (postprocessor.NeedsJitter()) camera_jitter = camera->Jitter(gfx->GetFrameIndex());

		if (camera->ViewProj() != frame_cbuf_data.prev_view_projection) path_tracer.Reset();
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
		frame_cbuf_data.display_resolution_x = (float)display_width;
		frame_cbuf_data.display_resolution_y = (float)display_height;
		frame_cbuf_data.render_resolution_x = (float)render_width;
		frame_cbuf_data.render_resolution_y = (float)render_height;
		frame_cbuf_data.delta_time = dt;
		frame_cbuf_data.total_time = total_time;
		frame_cbuf_data.frame_count = gfx->GetFrameIndex();
		frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
		frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;
		frame_cbuf_data.env_map_idx = sky_pass.GetSkyIndex();
		frame_cbuf_data.meshes_idx = (int32)scene_buffers[SceneBuffer_Mesh].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.materials_idx = (int32)scene_buffers[SceneBuffer_Material].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.instances_idx = (int32)scene_buffers[SceneBuffer_Instance].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.lights_idx = (int32)scene_buffers[SceneBuffer_Light].buffer_srv_gpu.GetIndex();
		shadow_renderer.FillFrameCBuffer(frame_cbuf_data);
		frame_cbuf_data.ddgi_volumes_idx = ddgi.IsEnabled() ? ddgi.GetDDGIVolumeIndex() : -1;
		frame_cbuf_data.printf_buffer_idx = gpu_debug_printer.GetPrintfBufferIndex();
		frame_cbuf_data.rain_splash_diffuse_idx = rain_pass.GetRainSplashDiffuseIndex();
		frame_cbuf_data.rain_splash_bump_idx = rain_pass.GetRainSplashBumpIndex();
		frame_cbuf_data.rain_blocker_map_idx = rain_pass.GetRainBlockerMapIndex();
		frame_cbuf_data.rain_view_projection = rain_pass.GetRainViewProjection();
		frame_cbuf_data.rain_total_time = rain_pass.GetRainTotalTime();

		if (ray_tracing_supported && reg.view<RayTracing>().size())
		{
			frame_cbuf_data.accel_struct_idx = accel_structure.GetTLASIndex();
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

	void Renderer::Render_Deferred(RenderGraph& render_graph)
	{
		if (update_picking_data)
		{
			picking_data = picking_pass.GetPickingData();
			update_picking_data = false;
		}
		if (rain_pass.IsEnabled()) rain_pass.AddBlockerPass(render_graph);
		if (gpu_driven_renderer.IsEnabled()) gpu_driven_renderer.AddPasses(render_graph);
		else gbuffer_pass.AddPass(render_graph);

		if(ddgi.IsEnabled()) ddgi.AddPasses(render_graph);

		decals_pass.AddPass(render_graph);
		postprocessor.AddAmbientOcclusionPass(render_graph);
		shadow_renderer.AddShadowMapPasses(render_graph);
		shadow_renderer.AddRayTracingShadowPasses(render_graph);

		if (renderer_output == RendererOutput::Final)
		{
			switch (lighting_path)
			{
			case LightingPathType::Deferred:			deferred_lighting_pass.AddPass(render_graph); break;
			case LightingPathType::TiledDeferred:		tiled_deferred_lighting_pass.AddPass(render_graph); break;
			case LightingPathType::ClusteredDeferred:	clustered_deferred_lighting_pass.AddPass(render_graph, true); break;
			}

			if (volumetric_lights > 0)
			{
				switch (volumetric_path)
				{
				case VolumetricPathType::Raymarching2D: volumetric_lighting_pass.AddPass(render_graph); break;
				case VolumetricPathType::FogVolume:		volumetric_fog_pass.AddPasses(render_graph); break;
				}
			}

			if (ddgi.IsEnabled() && ddgi.Visualize()) ddgi.AddVisualizePass(render_graph);
			ocean_renderer.AddPasses(render_graph);
			sky_pass.AddComputeSkyPass(render_graph, sun_direction);
			sky_pass.AddDrawSkyPass(render_graph);
			picking_pass.AddPass(render_graph);
			if (rain_pass.IsEnabled()) rain_pass.AddPass(render_graph);
			postprocessor.AddPasses(render_graph);
			g_DebugRenderer.Render(render_graph);
		}
		else
		{
			renderer_output_pass.AddPass(render_graph, renderer_output);
		}
	}
	void Renderer::Render_PathTracing(RenderGraph& render_graph)
	{
		path_tracer.AddPass(render_graph);
		postprocessor.AddTonemapPass(render_graph, RG_NAME(PT_Output));
	}

	void Renderer::GUI()
	{
		if (gpu_driven_renderer.IsSupported()) gpu_driven_renderer.GUI();
		if (ddgi.IsSupported()) ddgi.GUI();
		if (renderer_output == RendererOutput::Final)
		{
			if (lighting_path == LightingPathType::TiledDeferred) tiled_deferred_lighting_pass.GUI();
			switch (volumetric_path)
			{
			case VolumetricPathType::Raymarching2D: volumetric_lighting_pass.GUI(); break;
			case VolumetricPathType::FogVolume:		volumetric_fog_pass.GUI();		break;
			}
			ocean_renderer.GUI();
			sky_pass.GUI();
			rain_pass.GUI();
			QueueGUI([&]()
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
							static float sun_elevation = 75.0f;
							static float sun_azimuth = 260.0f;
							static float sun_temperature = 5900.0f;
							ConvertDirectionToAzimuthAndElevation(-sun_light->direction, sun_elevation, sun_azimuth);

							ImGui::SliderFloat("Sun Temperature", &sun_temperature, 1000.0f, 15000.0f);
							ImGui::SliderFloat("Sun Energy", &sun_light->intensity, 0.0f, 50.0f);
							ImGui::SliderFloat("Sun Elevation", &sun_elevation, -90.0f, 90.0f);
							ImGui::SliderFloat("Sun Azimuth", &sun_azimuth, 0.0f, 360.0f);

							sun_light->color = ConvertTemperatureToColor(sun_temperature);
							sun_light->direction = ConvertElevationAndAzimuthToDirection(sun_elevation, sun_azimuth);
							sun_light->position = 1e3 * sun_light->direction;
							sun_light->direction = -sun_light->direction;
							sun_transform->current_transform = XMMatrixTranslationFromVector(sun_light->position);
						}
						ImGui::TreePop();
					}
				}, GUICommandGroup_Renderer);
			QueueGUI([&]()
				{
					static int current_volumetric_path = (int)volumetric_path;
					if (ImGui::TreeNode("Misc"))
					{
						if (ImGui::Combo("Volumetric Fog", &current_volumetric_path, "None\0 Raymarching 2D\0Fog Volume\0", 3))
						{
							VolumetricPath->Set(current_volumetric_path);
						}
						if (!ddgi.IsEnabled())
						{
							ImGui::ColorEdit3("Ambient Color", ambient_color);
						}
						ImGui::SliderFloat3("Wind Direction", wind_dir, -1.0f, 1.0f);
						ImGui::SliderFloat("Wind Speed", &wind_speed, 0.0f, 32.0f);
						volumetric_path = static_cast<VolumetricPathType>(current_volumetric_path);
						ImGui::TreePop();
					}
				}, GUICommandGroup_Renderer);
		}
		postprocessor.GUI();
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

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT final_texture_footprint{};
		D3D12_RESOURCE_DESC d3d12_final_texture_desc = final_texture->GetNative()->GetDesc();
		gfx->GetDevice()->GetCopyableFootprints(&d3d12_final_texture_desc, 0, 1, 0, &final_texture_footprint, nullptr, nullptr, nullptr);

		if (!screenshot_buffer)
		{	
			GfxBufferDesc screenshot_desc{};
			screenshot_desc.size = final_texture_footprint.Footprint.RowPitch * final_texture_footprint.Footprint.Height;
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
			WriteImageToFile(FileType::PNG, name.data(), display_width, display_height,
							 screenshot_buffer->GetMappedData(), display_width * 4);
			screenshot_buffer.reset();
			ADRIA_LOG(INFO, "Screenshot %s.png saved to screenshots folder!", screenshot_name.c_str());
			screenshot_fence_value++;
			}, absolute_screenshot_path);

		take_screenshot = false;
	}

}

