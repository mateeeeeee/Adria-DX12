#include "Renderer.h"
#include "BlackboardData.h"
#include "Camera.h"
#include "Components.h"
#include "PSOCache.h"
#include "ShaderCache.h"
#include "SkyModel.h"
#include "TextureManager.h"
#include "DebugRenderer.h"

#include "entt/entity/registry.hpp"
#include "Editor/GUICommand.h"
#include "Editor/Editor.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxTexture.h"

#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxTracyProfiler.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/Random.h"
#include "Utilities/hwbp.h"
#include "Math/Halton.h"
#include "Logging/Logger.h"

using namespace DirectX;

namespace adria
{
	extern bool dump_render_graph;

	Renderer::Renderer(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height) : reg(reg), gfx(gfx), resource_pool(gfx),
		accel_structure(gfx), camera(nullptr), width(width), height(height),
		backbuffer_count(gfx->GetBackbufferCount()), backbuffer_index(gfx->GetBackbufferIndex()), final_texture(nullptr),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count), gpu_driven_renderer(reg, gfx, width, height),
		gbuffer_pass(reg, width, height), ambient_pass(width, height), tonemap_pass(width, height),
		sky_pass(reg, gfx, width, height), deferred_lighting_pass(width, height), volumetric_lighting_pass(width, height),
		tiled_deferred_lighting_pass(reg, width, height) , copy_to_texture_pass(width, height), add_textures_pass(width, height),
		postprocessor(reg, width, height), fxaa_pass(width, height), picking_pass(gfx, width, height),
		clustered_deferred_lighting_pass(reg, gfx, width, height), ssao_pass(width, height), hbao_pass(width, height),
		decals_pass(reg, width, height), ocean_renderer(reg, width, height),
		shadow_renderer(reg, gfx, width, height), rtao_pass(gfx, width, height), rtr_pass(gfx, width, height),
		path_tracer(gfx, width, height), ray_tracing_supported(gfx->GetCapabilities().SupportsRayTracing())
	{
		g_DebugRenderer.Initialize(width, height);
		g_GfxProfiler.Initialize(gfx);
		GfxTracyProfiler::Initialize(gfx);
		CreateSizeDependentResources();
		shadow_renderer.GetShadowTextureRenderedEvent().AddMember(&DeferredLightingPass::OnShadowTextureRendered, deferred_lighting_pass);
		shadow_renderer.GetShadowTextureRenderedEvent().AddMember(&VolumetricLightingPass::OnShadowTextureRendered, volumetric_lighting_pass);
	}

	Renderer::~Renderer()
	{
		GfxTracyProfiler::Destroy();
		g_GfxProfiler.Destroy();
		gfx->WaitForGPU();
		reg.clear();
		gfxcommon::Destroy();
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
		MiscGUI();
	}
	void Renderer::Render(RendererSettings const& _settings)
	{
		renderer_settings = _settings;

		RenderGraph render_graph(resource_pool);
		RGBlackboard& rg_blackboard = render_graph.GetBlackboard();
		FrameBlackboardData global_data{};
		{
			global_data.camera_position = camera->Position();
			global_data.camera_view = camera->View();
			global_data.camera_proj = camera->Proj();
			global_data.camera_viewproj = camera->ViewProj();
			global_data.camera_fov = camera->Fov();
			global_data.frame_cbuffer_address = frame_cbuffer.BufferLocation(backbuffer_index);
		}
		rg_blackboard.Add<FrameBlackboardData>(std::move(global_data));

		render_graph.ImportTexture(RG_RES_NAME(Backbuffer), gfx->GetBackbuffer());

		if (renderer_settings.render_path == RenderPathType::PathTracing) Render_PathTracing(render_graph);
		else Render_Deferred(render_graph);

		render_graph.Build();
		if (dump_render_graph) render_graph.DumpRenderGraph("rendergraph.gv");
		render_graph.Execute();
	}

	void Renderer::OnResize(uint32 w, uint32 h)
	{
		if (width != w || height != h)
		{
			width = w; height = h;
			CreateSizeDependentResources();
			g_DebugRenderer.OnResize(w, h);
			gbuffer_pass.OnResize(w, h);
			gpu_driven_renderer.OnResize(w, h);
			ambient_pass.OnResize(w, h);
			ssao_pass.OnResize(w, h);
			hbao_pass.OnResize(w, h);
			sky_pass.OnResize(w, h);
			deferred_lighting_pass.OnResize(w, h);
			volumetric_lighting_pass.OnResize(w, h);
			tiled_deferred_lighting_pass.OnResize(w, h);
			clustered_deferred_lighting_pass.OnResize(w, h);
			copy_to_texture_pass.OnResize(w, h);
			tonemap_pass.OnResize(w, h);
			fxaa_pass.OnResize(w, h);
			postprocessor.OnResize(gfx, w, h);
			add_textures_pass.OnResize(w, h);
			picking_pass.OnResize(w, h);
			decals_pass.OnResize(w, h);
			ocean_renderer.OnResize(w, h);
			shadow_renderer.OnResize(w, h);
			rtr_pass.OnResize(w, h);
			path_tracer.OnResize(w, h);
			rtao_pass.OnResize(w, h);
		}
	}
	void Renderer::OnSceneInitialized()
	{
		sky_pass.OnSceneInitialized(gfx);
		decals_pass.OnSceneInitialized(gfx);
		ssao_pass.OnSceneInitialized(gfx);
		hbao_pass.OnSceneInitialized(gfx);
		postprocessor.OnSceneInitialized(gfx);
		ocean_renderer.OnSceneInitialized(gfx);
		tonemap_pass.OnSceneInitialized(gfx);
		CreateAS();

		gfxcommon::Initialize(gfx);
		g_TextureManager.OnSceneInitialized();
	}
	void Renderer::OnRightMouseClicked(int32 x, int32 y)
	{
		update_picking_data = true;
	}

	void Renderer::CreateSizeDependentResources()
	{
		GfxTextureDesc ldr_desc{};
		ldr_desc.width = width;
		ldr_desc.height = height;
		ldr_desc.format = GfxFormat::R10G10B10A2_UNORM;
		ldr_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource | GfxBindFlag::RenderTarget;
		ldr_desc.initial_state = GfxResourceState::UnorderedAccess;

		final_texture = std::make_unique<GfxTexture>(gfx, ldr_desc);
	}
	void Renderer::CreateAS()
	{
		if (!ray_tracing_supported) return;
		if (reg.view<RayTracing>().size() == 0) return;

		auto ray_tracing_view = reg.view<Mesh, RayTracing>();
		for (auto entity : ray_tracing_view)
		{
			auto const& mesh = ray_tracing_view.get<Mesh>(entity);
			accel_structure.AddInstance(mesh);
		}
		accel_structure.Build();
	}

	void Renderer::UpdateSceneBuffers()
	{
		volumetric_lights = 0;
		for (auto e : reg.view<Batch>())
			reg.destroy(e);
		reg.clear<Batch>();

		std::vector<LightHLSL> hlsl_lights{};
		uint32 light_index = 0;
		Matrix light_transform = renderer_settings.render_path == RenderPathType::PathTracing ? Matrix::Identity : camera->View();
		for (auto light_entity : reg.view<Light>())
		{
			Light& light = reg.get<Light>(light_entity);
			light.light_index = light_index;
			++light_index;

			LightHLSL& hlsl_light = hlsl_lights.emplace_back();
			hlsl_light.color = light.color * light.energy;
			hlsl_light.position = XMVector4Transform(light.position, light_transform);
			hlsl_light.direction = XMVector4Transform(light.direction, light_transform);
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

		std::vector<MeshHLSL> meshes;
		std::vector<InstanceHLSL> instances;
		std::vector<MaterialHLSL> materials;
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

				InstanceHLSL& instance_hlsl = instances.emplace_back();
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
				MeshHLSL& mesh_hlsl = meshes.emplace_back();
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
				MaterialHLSL& material_hlsl = materials.emplace_back();
				material_hlsl.diffuse_idx = (uint32)material.albedo_texture;
				material_hlsl.normal_idx = (uint32)material.normal_texture;
				material_hlsl.roughness_metallic_idx = (uint32)material.metallic_roughness_texture;
				material_hlsl.emissive_idx = (uint32)material.emissive_texture;
				material_hlsl.base_color_factor = XMFLOAT3(material.base_color);
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
				scene_buffer.buffer = std::make_unique<GfxBuffer>(gfx, StructuredBufferDesc<T>(data.size(), false, true));
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

		size_t count = reg.size();
	}

	void Renderer::UpdateFrameConstants(float dt)
	{
		static float total_time = 0.0f;
		total_time += dt;

		float jitter_x = 0.0f, jitter_y = 0.0f;
		if (HasAllFlags(renderer_settings.postprocess.anti_aliasing, AntiAliasing_TAA))
		{
			constexpr HaltonSequence<16, 2> x;
			constexpr HaltonSequence<16, 3> y;
			jitter_x = x[gfx->GetFrameIndex() % 16];
			jitter_y = y[gfx->GetFrameIndex() % 16];
			jitter_x = ((jitter_x - 0.5f) / width) * 2;
			jitter_y = ((jitter_y - 0.5f) / height) * 2;
		}

		static FrameCBuffer frame_cbuf_data{};
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
		frame_cbuf_data.camera_jitter_x = jitter_x;
		frame_cbuf_data.camera_jitter_y = jitter_y;
		frame_cbuf_data.screen_resolution_x = (float)width;
		frame_cbuf_data.screen_resolution_y = (float)height;
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

		auto lights = reg.view<Light>();
		for (auto light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (light_data.type == LightType::Directional && light_data.active)
			{
				frame_cbuf_data.sun_direction = -light_data.direction;
				frame_cbuf_data.sun_color = light_data.color * light_data.energy;
				sun_direction = light_data.direction;
				break;
			}
		}

		if (ray_tracing_supported && reg.view<RayTracing>().size())
		{
			frame_cbuf_data.accel_struct_idx = accel_structure.GetTLASIndex();
		}

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
			batch.camera_visibility = true;// camera_frustum.Intersects(aabb);
		}
	}

	void Renderer::Render_Deferred(RenderGraph& render_graph)
	{
		if (update_picking_data)
		{
			picking_data = picking_pass.GetPickingData();
			update_picking_data = false;
		}
		sky_pass.AddComputeSkyPass(render_graph, sun_direction);

		if (gfx->GetCapabilities().SupportsMeshShaders()) gpu_driven_renderer.Render(render_graph);
		else gbuffer_pass.AddPass(render_graph);

		decals_pass.AddPass(render_graph);
		switch (renderer_settings.postprocess.ambient_occlusion)
		{
		case AmbientOcclusion::SSAO:
		{
			ssao_pass.AddPass(render_graph);
			break;
		}
		case AmbientOcclusion::HBAO:
		{
			hbao_pass.AddPass(render_graph);
			break;
		}
		case AmbientOcclusion::RTAO:
		{
			rtao_pass.AddPass(render_graph);
			break;
		}
		case AmbientOcclusion::None:
		default:
			break;
		}
		ambient_pass.AddPass(render_graph);
		shadow_renderer.AddShadowMapPasses(render_graph);
		shadow_renderer.AddRayTracingShadowPasses(render_graph);
		switch (renderer_settings.render_path)
		{
		case RenderPathType::RegularDeferred:
			deferred_lighting_pass.AddPass(render_graph);
			break;
		case RenderPathType::TiledDeferred:
			tiled_deferred_lighting_pass.AddPass(render_graph);
			break;
		case RenderPathType::ClusteredDeferred:
			clustered_deferred_lighting_pass.AddPass(render_graph, true);
			break;
		}
		if (volumetric_lights > 0) volumetric_lighting_pass.AddPass(render_graph);

		ocean_renderer.AddPasses(render_graph);
		sky_pass.AddDrawSkyPass(render_graph);
		picking_pass.AddPass(render_graph);
		if (renderer_settings.postprocess.reflections == Reflections::RTR) rtr_pass.AddPass(render_graph);
		postprocessor.AddPasses(render_graph, renderer_settings.postprocess);
		render_graph.ImportTexture(RG_RES_NAME(FinalTexture), final_texture.get());
		ResolveToFinalTexture(render_graph);
		g_DebugRenderer.Render(render_graph);

		if (!g_Editor.IsActive()) CopyToBackbuffer(render_graph);
		else g_Editor.AddRenderPass(render_graph);
	}
	void Renderer::Render_PathTracing(RenderGraph& render_graph)
	{
		path_tracer.AddPass(render_graph);
		render_graph.ImportTexture(RG_RES_NAME(FinalTexture), final_texture.get());
		tonemap_pass.AddPass(render_graph, RG_RES_NAME(PT_Output));
		if (!g_Editor.IsActive()) CopyToBackbuffer(render_graph);
		else g_Editor.AddRenderPass(render_graph);
	}

	void Renderer::MiscGUI()
	{
		//todo
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
				data.dst = builder.WriteCopyDstTexture(RG_RES_NAME(Backbuffer));
				data.src = builder.ReadCopySrcTexture(RG_RES_NAME(FinalTexture));
			},
			[=](CopyToBackbufferPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture const& src_texture = ctx.GetCopySrcTexture(data.src);
				GfxTexture& dst_texture = ctx.GetCopyDstTexture(data.dst);
				cmd_list->CopyTexture(dst_texture, src_texture);
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);
	}
	void Renderer::ResolveToFinalTexture(RenderGraph& rg)
	{
		RGResourceName final_texture = postprocessor.GetFinalResource();
		if (HasAnyFlag(renderer_settings.postprocess.anti_aliasing, AntiAliasing_FXAA))
		{
			tonemap_pass.AddPass(rg, final_texture, RG_RES_NAME(TonemapOutput));
			fxaa_pass.AddPass(rg, RG_RES_NAME(TonemapOutput));
		}
		else
		{
			tonemap_pass.AddPass(rg, final_texture);
		}
	}
}

