#include <filesystem>
#include "nfd.h"
#include "Editor.h"
#include "ImGuiManager.h"
#include "EditorLogger.h"
#include "EditorConsole.h"
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Core/Paths.h"
#include "IconsFontAwesome6.h"
#include "Rendering/Renderer.h"
#include "Rendering/Camera.h"
#include "Rendering/EntityLoader.h"
#include "Rendering/ShaderManager.h"
#include "Rendering/DebugRenderer.h"
#include "Rendering/HelperPasses.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxProfiler.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/FilesUtil.h"
#include "Utilities/StringUtil.h"
#include "Utilities/Random.h"
#include "Math/BoundingVolumeUtil.h"

using namespace DirectX;
namespace fs = std::filesystem;

namespace adria
{
	extern bool dump_render_graph;

	struct ProfilerState
	{
		bool  show_average = false;
		struct AccumulatedTimeStamp
		{
			float sum;
			float minimum;
			float maximum;

			AccumulatedTimeStamp()
				: sum(0.0f), minimum(FLT_MAX), maximum(0)
			{}
		};

		std::vector<AccumulatedTimeStamp> displayed_timestamps;
		std::vector<AccumulatedTimeStamp> accumulating_timestamps;
		double last_reset_time = 0.0;
		uint32 accumulating_frame_count = 0;
	};

	Editor::Editor() = default;
	Editor::~Editor() = default;
	void Editor::Init(EditorInit&& init)
	{
		logger = new EditorLogger();
		g_Log.Register(logger);
		engine = std::make_unique<Engine>(init.engine_init);
		gfx = engine->gfx.get();
		gui = std::make_unique<ImGuiManager>(gfx);
		engine->RegisterEditorEventCallbacks(editor_events);

		console = std::make_unique<EditorConsole>();
		ray_tracing_supported = gfx->GetCapabilities().SupportsRayTracing();
		selected_entity = entt::null;
		SetStyle();

		fs::create_directory(paths::PixCapturesDir);
	}
	void Editor::Destroy()
	{
		gui.reset();
		engine.reset();
		console.reset();
	}
	void Editor::OnWindowEvent(WindowEventData const& msg_data)
	{
		engine->OnWindowEvent(msg_data);
		gui->OnWindowEvent(msg_data);
	}
	void Editor::Run()
	{
		HandleInput();
		if (gui->IsVisible()) engine->SetViewportData(&viewport_data);
		else engine->SetViewportData(nullptr);

		engine->Run();

		if (reload_shaders)
		{
			gfx->WaitForGPU();
			ShaderManager::CheckIfShadersHaveChanged();
			reload_shaders = false;
		}
	}
	bool Editor::IsActive() const
	{
		return gui->IsVisible();
	}

	void Editor::AddCommand(GUICommand&& command)
	{
		commands.emplace_back(std::move(command));
	}
	void Editor::AddDebugTexture(GUITexture&& debug_texture)
	{
		debug_textures.emplace_back(std::move(debug_texture));
	}
	void Editor::AddRenderPass(RenderGraph& rg)
	{
		struct EditorPassData
		{
			RGTextureReadOnlyId src;
			RGRenderTargetId rt;
		};

		rg.AddPass<EditorPassData>("Editor Pass",
			[=](EditorPassData& data, RenderGraphBuilder& builder)
			{
				data.src = builder.ReadTexture(RG_NAME(FinalTexture));
				data.rt = builder.WriteRenderTarget(RG_NAME(Backbuffer), RGLoadStoreAccessOp::Preserve_Preserve);
				Vector2u display_resolution = engine->renderer->GetDisplayResolution();
				builder.SetViewport(display_resolution.x, display_resolution.y);
			},
			[=](EditorPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDescriptor src_descriptor = ctx.GetReadOnlyTexture(data.src);
				gui->Begin();
				{
					ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
					MenuBar();
					Scene(src_descriptor);
					ListEntities();
					AddEntities();
					Settings();
					Camera();
					Properties();
					Log();
					Console();
					Profiling();
					ShaderHotReload();
					Debug();
				}
				gui->End(cmd_list);
				commands.clear();
				debug_textures.clear();
			}, RGPassType::Graphics, RGPassFlags::ForceNoCull | RGPassFlags::LegacyRenderPass);

	}

	void Editor::SetStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::StyleColorsDark(&style);

		style.FrameRounding = 0.0f;
		style.GrabRounding = 1.0f;
		style.WindowRounding = 0.0f;
		style.IndentSpacing = 10.0f;
		style.WindowPadding = ImVec2(5, 5);
		style.FramePadding = ImVec2(2, 2);
		style.WindowBorderSize = 1.00f;
		style.ChildBorderSize = 1.00f;
		style.PopupBorderSize = 1.00f;
		style.FrameBorderSize = 1.00f;
		style.ScrollbarSize = 20.0f;
		style.WindowMenuButtonPosition = ImGuiDir_Right;

		ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(0.85f, 0.87f, 0.91f, 0.88f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.49f, 0.50f, 0.53f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
		colors[ImGuiCol_Border] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.09f, 0.09f, 0.09f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.56f, 0.74f, 0.73f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.23f, 0.26f, 0.32f, 0.60f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.63f, 0.76f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.51f, 0.63f, 0.76f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.51f, 0.63f, 0.76f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.74f, 0.73f, 1.00f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.53f, 0.75f, 0.82f, 0.86f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.61f, 0.74f, 0.87f, 1.00f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.24f, 0.31f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.17f, 0.19f, 0.23f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.56f, 0.74f, 0.73f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.53f, 0.75f, 0.82f, 0.86f);
	}
	void Editor::HandleInput()
	{
		if (scene_focused && g_Input.IsKeyDown(KeyCode::I))
		{
			gui->ToggleVisibility();
			g_Input.SetMouseVisibility(gui->IsVisible());
		}
		if (scene_focused && g_Input.IsKeyDown(KeyCode::G)) gizmo_enabled = !gizmo_enabled;
		if (g_Input.IsKeyDown(KeyCode::Tilde)) show_basic_console = !show_basic_console;
		if (gizmo_enabled && gui->IsVisible())
		{
			if (g_Input.IsKeyDown(KeyCode::T)) gizmo_op = ImGuizmo::TRANSLATE;
			if (g_Input.IsKeyDown(KeyCode::R)) gizmo_op = ImGuizmo::ROTATE;
			if (g_Input.IsKeyDown(KeyCode::E)) gizmo_op = ImGuizmo::SCALE;
		}
		if (gui->IsVisible()) engine->camera->Enable(scene_focused);
		else engine->camera->Enable(true);
	}
	void Editor::MenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu(ICON_FA_FILE" File"))
			{
				if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN" Load Model"))
				{
					nfdchar_t* file_path = NULL;
					const nfdchar_t* filter_list = "gltf";
					nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
					if (result == NFD_OKAY)
					{
						std::string model_path = file_path;

						ModelParameters params{};
						params.model_path = model_path;
						std::string texture_path = GetParentPath(model_path);
						if (!texture_path.empty()) texture_path.append("/");

						params.textures_path = texture_path;
						engine->entity_loader->ImportModel_GLTF(params);
						free(file_path);
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(ICON_FA_WINDOW_MAXIMIZE "Windows"))
			{
				if (ImGui::MenuItem(ICON_FA_CLOCK" Profiler", 0, visibility_flags[Flag_Profiler]))			 visibility_flags[Flag_Profiler] = !visibility_flags[Flag_Profiler];
				if (ImGui::MenuItem(ICON_FA_COMMENT" Log", 0, visibility_flags[Flag_Log]))					 visibility_flags[Flag_Log] = !visibility_flags[Flag_Log];
				if (ImGui::MenuItem(ICON_FA_TERMINAL" Console ", 0, visibility_flags[Flag_Console]))		 visibility_flags[Flag_Console] = !visibility_flags[Flag_Console];
				if (ImGui::MenuItem(ICON_FA_CAMERA" Camera", 0, visibility_flags[Flag_Camera]))				 visibility_flags[Flag_Camera] = !visibility_flags[Flag_Camera];
				if (ImGui::MenuItem(ICON_FA_LIST "Entities", 0, visibility_flags[Flag_Entities]))			 visibility_flags[Flag_Entities] = !visibility_flags[Flag_Entities];
				if (ImGui::MenuItem(ICON_FA_FIRE" Hot Reload", 0, visibility_flags[Flag_HotReload]))		 visibility_flags[Flag_HotReload] = !visibility_flags[Flag_HotReload];
				if (ImGui::MenuItem(ICON_FA_GEAR" Settings", 0, visibility_flags[Flag_Settings]))			 visibility_flags[Flag_Settings] = !visibility_flags[Flag_Settings];
				if (ImGui::MenuItem(ICON_FA_BUG" Debug", 0, visibility_flags[Flag_Debug]))					 visibility_flags[Flag_Debug] = !visibility_flags[Flag_Debug];
				if (ImGui::MenuItem(ICON_FA_PLUS "Add Entities", 0, visibility_flags[Flag_AddEntities]))	 visibility_flags[Flag_AddEntities] = !visibility_flags[Flag_AddEntities];

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(ICON_FA_QUESTION" Help"))
			{
				ImGui::Text("Controls\n");
				ImGui::Text(
					"Move Camera with W, A, S, D, Q and E. Hold Right Click and move Mouse for rotating Camera. Use Mouse Scroll for Zoom In/Out.\n"
					"Press I to toggle between Cinema Mode and Editor Mode. (Scene Window has to be active) \n"
					"Press G to toggle Gizmo. (Scene Window has to be active) \n"
					"When Gizmo is enabled, use T, R and E to switch between Translation, Rotation and Scaling Mode.\n"
					"To hot-reload shaders, press F5."
				);
				ImGui::Spacing();

				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void Editor::AddEntities()
	{
		if (!visibility_flags[Flag_AddEntities]) return;
		if (ImGui::Begin("Add Entities", &visibility_flags[Flag_AddEntities]))
		{
			if (ImGui::TreeNodeEx("Point Lights", 0))
			{
				static int light_count_to_add = 1;
				ImGui::SliderInt("Light Count", &light_count_to_add, 1, 128);
				if (ImGui::Button("Create Random Point Lights"))
				{
					static RealRandomGenerator real(0.0f, 1.0f);

					for (int32 i = 0; i < light_count_to_add; ++i)
					{
						LightParameters light_params{};
						light_params.light_data.casts_shadows = false;
						light_params.light_data.color = Vector4(real() * 2, real() * 2, real() * 2, 1.0f);
						light_params.light_data.direction = Vector4(0.5f, -1.0f, 0.1f, 0.0f);
						light_params.light_data.position = Vector4(real() * 200 - 100, real() * 200.0f, real() * 200 - 100, 1.0f);
						light_params.light_data.type = LightType::Point;
						light_params.mesh_type = LightMesh::NoMesh;
						light_params.light_data.range = real() * 100.0f + 40.0f;
						light_params.light_data.active = true;
						light_params.light_data.volumetric = false;
						light_params.light_data.volumetric_strength = 0.004f;
						engine->entity_loader->LoadLight(light_params);
					}
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if (ImGui::TreeNodeEx("Spot Lights", 0))
			{
				static int light_count_to_add = 1;
				ImGui::SliderInt("Light Count", &light_count_to_add, 1, 128);
				if (ImGui::Button("Create Random Spot Lights"))
				{
					static RealRandomGenerator real(0.0f, 1.0f);

					for (int32 i = 0; i < light_count_to_add; ++i)
					{
						LightParameters light_params{};
						light_params.light_data.casts_shadows = false;
						light_params.light_data.inner_cosine = real();
						light_params.light_data.outer_cosine = real();
						light_params.light_data.color = Vector4(real() * 2, real() * 2, real() * 2, 1.0f);
						light_params.light_data.direction = Vector4(0.5f, -1.0f, 0.1f, 0.0f);
						light_params.light_data.position = Vector4(real() * 200 - 100, real() * 200.0f, real() * 200 - 100, 1.0f);
						light_params.light_data.type = LightType::Spot;
						light_params.mesh_type = LightMesh::NoMesh;
						light_params.light_data.range = real() * 100.0f + 40.0f;
						light_params.light_data.active = true;
						light_params.light_data.volumetric = false;
						light_params.light_data.volumetric_strength = 0.004f;
						if (light_params.light_data.inner_cosine > light_params.light_data.outer_cosine)
							std::swap(light_params.light_data.inner_cosine, light_params.light_data.outer_cosine);
						engine->entity_loader->LoadLight(light_params);
					}
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if (ImGui::TreeNodeEx("Ocean", 0))
			{
				static GridParameters ocean_params{};
				static int32 tile_count[2] = { 512, 512 };
				static float tile_size[2] = { 40.0f, 40.0f };
				static float texture_scale[2] = { 20.0f, 20.0f };

				ImGui::SliderInt2("Tile Count", tile_count, 32, 1024);
				ImGui::SliderFloat2("Tile Size", tile_size, 1.0, 100.0f);
				ImGui::SliderFloat2("Texture Scale", texture_scale, 0.1f, 10.0f);

				ocean_params.tile_count_x = tile_count[0];
				ocean_params.tile_count_z = tile_count[1];
				ocean_params.tile_size_x = tile_size[0];
				ocean_params.tile_size_z = tile_size[1];
				ocean_params.texture_scale_x = texture_scale[0];
				ocean_params.texture_scale_z = texture_scale[1];

				if (ImGui::Button("Load Ocean"))
				{
					OceanParameters params{};
					params.ocean_grid = std::move(ocean_params);
					gfx->WaitForGPU();
					engine->entity_loader->LoadOcean(params);
				}

				if (ImGui::Button(ICON_FA_ERASER" Clear"))
				{
					for (auto e : engine->reg.view<Ocean>()) engine->reg.destroy(e);
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if (ImGui::TreeNodeEx("Decals", 0))
			{
				static DecalParameters params{};
				static char NAME_BUFFER[128];
				ImGui::InputText("Name", NAME_BUFFER, sizeof(NAME_BUFFER));
				params.name = std::string(NAME_BUFFER);
				ImGui::PushID(6);
				if (ImGui::Button("Select Albedo Texture"))
				{
					nfdchar_t* file_path = NULL;
					nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
					nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
					if (result == NFD_OKAY)
					{
						std::string texture_path = file_path;
						params.albedo_texture_path = texture_path;
						free(file_path);
					}
				}
				ImGui::PopID();
				ImGui::Text(params.albedo_texture_path.c_str());

				ImGui::PushID(7);
				if (ImGui::Button("Select Normal Texture"))
				{
					nfdchar_t* file_path = NULL;
					nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
					nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
					if (result == NFD_OKAY)
					{
						std::string texture_path = file_path;
						params.normal_texture_path = texture_path;
						free(file_path);
					}
				}

				ImGui::PopID();
				ImGui::Text(params.normal_texture_path.c_str());

				ImGui::DragFloat("Size", &params.size, 2.0f, 10.0f, 200.0f);
				ImGui::DragFloat("Rotation", &params.rotation, 1.0f, -180.0f, 180.0f);
				ImGui::Checkbox("Modify GBuffer Normals", &params.modify_gbuffer_normals);

				auto const& picking_data = engine->renderer->GetPickingData();
				ImGui::Text("Picked Position: %f %f %f", picking_data.position.x, picking_data.position.y, picking_data.position.z);
				ImGui::Text("Picked Normal: %f %f %f", picking_data.normal.x, picking_data.normal.y, picking_data.normal.z);
				if (ImGui::Button("Load Decal"))
				{
					params.position = Vector3(picking_data.position);
					params.normal = Vector3(picking_data.normal);
					params.rotation = XMConvertToRadians(params.rotation);

					engine->entity_loader->LoadDecal(params);
				}
				if (ImGui::Button(ICON_FA_ERASER" Clear Decals"))
				{
					for (auto e : engine->reg.view<Decal>()) engine->reg.destroy(e);
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
		}
		ImGui::End();
	}
	void Editor::ListEntities()
	{
		if (!visibility_flags[Flag_Entities]) return;
		auto all_entities = engine->reg.view<Tag>();
		if (ImGui::Begin(ICON_FA_LIST" Entities ", &visibility_flags[Flag_Entities]))
		{
			std::vector<entt::entity> deleted_entities{};
			std::function<void(entt::entity, bool)> ShowEntity;
			ShowEntity = [&](entt::entity e, bool first_iteration)
			{
				auto& tag = all_entities.get<Tag>(e);

				ImGuiTreeNodeFlags flags = ((selected_entity == e) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
				flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
				bool opened = ImGui::TreeNodeEx(tag.name.c_str(), flags);

				if (ImGui::IsItemClicked())
				{
					if (e == selected_entity) selected_entity = entt::null;
					else selected_entity = e;
				}

				if (opened)
				{
					ImGui::TreePop();
				}
			};
			for (auto e : all_entities) ShowEntity(e, true);
		}
		ImGui::End();
	}
	void Editor::Properties()
	{
		if (!visibility_flags[Flag_Entities]) return;
		if (ImGui::Begin("Properties", &visibility_flags[Flag_Entities]))
		{
			GfxDevice* gfx = engine->gfx.get();
			if (selected_entity != entt::null)
			{
				auto tag = engine->reg.try_get<Tag>(selected_entity);
				if (tag)
				{
					char buffer[256];
					memset(buffer, 0, sizeof(buffer));
					std::strncpy(buffer, tag->name.c_str(), sizeof(buffer));
					if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
						tag->name = std::string(buffer);
				}

				auto light = engine->reg.try_get<Light>(selected_entity);
				if (light && ImGui::CollapsingHeader("Light"))
				{
					if (light->type == LightType::Directional)	ImGui::Text("Directional Light");
					else if (light->type == LightType::Spot)	ImGui::Text("Spot Light");
					else if (light->type == LightType::Point)	ImGui::Text("Point Light");

					float color[3] = { light->color.x, light->color.y, light->color.z };
					ImGui::ColorEdit3("Light Color", color);
					light->color = Vector4(color[0], color[1], color[2], 1.0f);

					ImGui::SliderFloat("Light Intensity", &light->intensity, 0.0f, 50.0f);

					if (engine->reg.all_of<Material>(selected_entity))
					{
						auto& material = engine->reg.get<Material>(selected_entity);
						memcpy(material.base_color, color, 3 * sizeof(float));
					}

					if (light->type == LightType::Directional || light->type == LightType::Spot)
					{
						float direction[3] = { light->direction.x, light->direction.y, light->direction.z };
						ImGui::SliderFloat3("Light direction", direction, -1.0f, 1.0f);
						light->direction = Vector4(direction[0], direction[1], direction[2], 0.0f);
						if (light->type == LightType::Directional)
						{
							light->position = -light->direction * 1e3;
						}
					}

					if (light->type == LightType::Spot)
					{
						float inner_angle = XMConvertToDegrees(acos(light->inner_cosine))
							, outer_angle = XMConvertToDegrees(acos(light->outer_cosine));
						ImGui::SliderFloat("Inner Spot Angle", &inner_angle, 0.0f, 90.0f);
						ImGui::SliderFloat("Outer Spot Angle", &outer_angle, inner_angle, 90.0f);

						light->inner_cosine = cos(XMConvertToRadians(inner_angle));
						light->outer_cosine = cos(XMConvertToRadians(outer_angle));
					}

					if (light->type == LightType::Point || light->type == LightType::Spot)
					{
						float position[3] = { light->position.x,  light->position.y,  light->position.z };
						ImGui::SliderFloat3("Light position", position, -300.0f, 500.0f);
						light->position = Vector4(position[0], position[1], position[2], 1.0f);
						ImGui::SliderFloat("Range", &light->range, 50.0f, 1000.0f);
					}

					if (engine->reg.all_of<Transform>(selected_entity))
					{
						auto& tr = engine->reg.get<Transform>(selected_entity);
						tr.current_transform = XMMatrixTranslationFromVector(light->position);
					}

					ImGui::Checkbox("Active", &light->active);

					if (light->type == LightType::Directional)
					{
						static int current_shadow_type = light->casts_shadows;
						ImGui::Combo("Shadow Technique", &current_shadow_type, "None\0Shadow Map\0Ray Traced Shadows\0", 3);
						if (!ray_tracing_supported && current_shadow_type == 2) current_shadow_type = 1;

						light->casts_shadows = (current_shadow_type == 1);
						light->ray_traced_shadows = (current_shadow_type == 2);
					}
					else
					{
						ImGui::Checkbox("Casts Shadows", &light->casts_shadows);
					}

					if (light->casts_shadows)
					{
						if (light->type == LightType::Directional)
						{
							ImGui::Checkbox("Use Cascades", &light->use_cascades);
						}
						ImGui::Checkbox("Screen Space Contact Shadows", &light->sscs);
						if (light->sscs)
						{
							ImGui::SliderFloat("Thickness", &light->sscs_thickness, 0.0f, 1.0f);
							ImGui::SliderFloat("Max Ray Distance", &light->sscs_max_ray_distance, 0.0f, 0.3f);
							ImGui::SliderFloat("Max Depth Distance", &light->sscs_max_depth_distance, 0.0f, 500.0f);
						}
					}

					ImGui::Checkbox("God Rays", &light->god_rays);
					if (light->god_rays)
					{
						ImGui::SliderFloat("God Rays Decay", &light->godrays_decay, 0.0f, 1.0f);
						ImGui::SliderFloat("God Rays Weight", &light->godrays_weight, 0.0f, 1.0f);
						ImGui::SliderFloat("God Rays Density", &light->godrays_density, 0.1f, 2.0f);
						ImGui::SliderFloat("God Rays Exposure", &light->godrays_exposure, 0.1f, 10.0f);
					}

					ImGui::Checkbox("Volumetric Lighting", &light->volumetric);
					if (light->volumetric)
					{
						ImGui::SliderFloat("Volumetric lighting Strength", &light->volumetric_strength, 0.0f, 0.1f);
					}

					ImGui::Checkbox("Lens Flare", &light->lens_flare);
				}

				auto material = engine->reg.try_get<Material>(selected_entity);
				if (material && ImGui::CollapsingHeader("Material"))
				{
					ImGui::Text("Albedo Texture");
					if (material->albedo_texture != INVALID_TEXTURE_HANDLE)
					{
						GfxDescriptor tex_handle = g_TextureManager.GetSRV(material->albedo_texture);
						GfxDescriptor dst_descriptor = gui->AllocateDescriptorsGPU();
						gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
						ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, ImVec2(48.0f, 48.0f));
					}

					ImGui::PushID(0);
					if (ImGui::Button("Remove"))
					{
						material->albedo_texture = INVALID_TEXTURE_HANDLE;
					}
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							material->albedo_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::Text("Metallic-Roughness Texture");
					if (material->metallic_roughness_texture != INVALID_TEXTURE_HANDLE)
					{
						GfxDescriptor tex_handle = g_TextureManager.GetSRV(material->metallic_roughness_texture);
						GfxDescriptor dst_descriptor = gui->AllocateDescriptorsGPU();
						gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
						ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
							ImVec2(48.0f, 48.0f));
					}


					ImGui::PushID(1);
					if (ImGui::Button("Remove"))
					{
						material->metallic_roughness_texture = INVALID_TEXTURE_HANDLE;
					}
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							material->metallic_roughness_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::Text("Emissive Texture");
					if (material->emissive_texture != INVALID_TEXTURE_HANDLE)
					{
						GfxDescriptor tex_handle = g_TextureManager.GetSRV(material->emissive_texture);
						GfxDescriptor dst_descriptor = gui->AllocateDescriptorsGPU();
						gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
						ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
							ImVec2(48.0f, 48.0f));
					}

					ImGui::PushID(2);
					if (ImGui::Button("Remove"))
					{
						material->emissive_texture = INVALID_TEXTURE_HANDLE;
					}
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							material->emissive_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::ColorEdit3("Base Color", material->base_color);
					ImGui::SliderFloat("Metallic Factor", &material->metallic_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Roughness Factor", &material->roughness_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Emissive Factor", &material->emissive_factor, 0.0f, 32.0f);
				}

				auto transform = engine->reg.try_get<Transform>(selected_entity);
				if (transform && ImGui::CollapsingHeader("Transform"))
				{
					Matrix tr = transform->current_transform;
					
					float translation[3], rotation[3], scale[3];
					ImGuizmo::DecomposeMatrixToComponents(tr.m[0], translation, rotation, scale);
					bool change = ImGui::InputFloat3("Translation", translation);
					change &= ImGui::InputFloat3("Rotation", rotation);
					change &= ImGui::InputFloat3("Scale", scale);
					ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, tr.m[0]);

					transform->current_transform = tr;
				}

				auto decal = engine->reg.try_get<Decal>(selected_entity);
				if (decal && ImGui::CollapsingHeader("Decal"))
				{
					ImGui::Text("Decal Albedo Texture");
					GfxDescriptor tex_handle = g_TextureManager.GetSRV(decal->albedo_decal_texture);
					GfxDescriptor dst_descriptor = gui->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(4);
					if (ImGui::Button("Remove")) decal->albedo_decal_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							decal->albedo_decal_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::Text("Decal Normal Texture");
					tex_handle = g_TextureManager.GetSRV(decal->normal_decal_texture);
					dst_descriptor = gui->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(5);
					if (ImGui::Button("Remove")) decal->normal_decal_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							decal->normal_decal_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();
					ImGui::Checkbox("Modify GBuffer Normals", &decal->modify_gbuffer_normals);
				}

				auto skybox = engine->reg.try_get<Skybox>(selected_entity);
				if (skybox && ImGui::CollapsingHeader("Skybox"))
				{
					ImGui::Checkbox("Active", &skybox->active);
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							skybox->cubemap_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
				}
			}
		}
		ImGui::End();
	}
	void Editor::Camera()
	{
		if (!visibility_flags[Flag_Camera]) return;

		auto& camera = *engine->camera;
		if (ImGui::Begin(ICON_FA_CAMERA" Camera", &visibility_flags[Flag_Camera]))
		{
			Vector3 cam_pos = camera.Position();
			ImGui::SliderFloat3("Position", (float*)&cam_pos, 0.0f, 2000.0f);
			camera.SetPosition(cam_pos);
			float near_plane = camera.Near(), far_plane = camera.Far();
			float fov = camera.Fov();
			ImGui::SliderFloat("Near", &near_plane, 10.0f, 3000.0f);
			ImGui::SliderFloat("Far", &far_plane, 0.001f, 2.0f);
			ImGui::SliderFloat("FOV", &fov, 0.01f, 1.5707f);
			camera.SetNearAndFar(near_plane, far_plane);
			camera.SetFov(fov);
			Vector3 look_at = camera.Forward();
			ImGui::Text("Look Vector: (%f,%f,%f)", look_at.x, look_at.y, look_at.z);
			
		}
		ImGui::End();
	}
	void Editor::Scene(GfxDescriptor& src)
	{
		ImGui::Begin(ICON_FA_GLOBE" Scene", nullptr, ImGuiWindowFlags_MenuBar);
		{
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Lighting Path"))
				{
					LightingPathType current_path = engine->renderer->GetLightingPath();
					auto AddMenuItem = [&](LightingPathType lighting_path, char const* item_name)
					{
						if (ImGui::MenuItem(item_name, nullptr, lighting_path == current_path)) { engine->renderer->SetLightingPath(lighting_path); }
					};
				#define AddLightingPathMenuItem(name) AddMenuItem(LightingPathType::##name, #name)
					AddLightingPathMenuItem(Deferred);
					AddLightingPathMenuItem(TiledDeferred);
					AddLightingPathMenuItem(ClusteredDeferred);
					AddLightingPathMenuItem(PathTracing);
				#undef AddLightingPathMenuItem
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Renderer Output"))
				{
					RendererOutput current_output = engine->renderer->GetRendererOutput();
					auto AddMenuItem = [&](RendererOutput output, char const* item_name)
					{
						if (ImGui::MenuItem(item_name, nullptr, output == current_output)) { engine->renderer->SetRendererOutput(output); }
					};

				#define AddRendererOutputMenuItem(name) AddMenuItem(RendererOutput::##name, #name)
					AddRendererOutputMenuItem(Final);
					AddRendererOutputMenuItem(Diffuse);
					AddRendererOutputMenuItem(WorldNormal);
					AddRendererOutputMenuItem(Roughness);
					AddRendererOutputMenuItem(Metallic);
					AddRendererOutputMenuItem(Emissive);
					AddRendererOutputMenuItem(AmbientOcclusion);
					AddRendererOutputMenuItem(IndirectLighting);
				#undef AddRendererOutputMenuItem
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			ImVec2 v_min = ImGui::GetWindowContentRegionMin();
			ImVec2 v_max = ImGui::GetWindowContentRegionMax();
			v_min.x += ImGui::GetWindowPos().x;
			v_min.y += ImGui::GetWindowPos().y;
			v_max.x += ImGui::GetWindowPos().x;
			v_max.y += ImGui::GetWindowPos().y;
			ImVec2 size(v_max.x - v_min.x, v_max.y - v_min.y);

			GfxDescriptor dst_descriptor = gui->AllocateDescriptorsGPU();
			gfx->CopyDescriptors(1, dst_descriptor, src);
			ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);

			scene_focused = ImGui::IsWindowFocused();

			ImVec2 mouse_pos = ImGui::GetMousePos();
			viewport_data.mouse_position_x = mouse_pos.x;
			viewport_data.mouse_position_y = mouse_pos.y;
			viewport_data.scene_viewport_focused = scene_focused;
			viewport_data.scene_viewport_pos_x = v_min.x;
			viewport_data.scene_viewport_pos_y = v_min.y;
			viewport_data.scene_viewport_size_x = size.x;
			viewport_data.scene_viewport_size_y = size.y;
		}

		if (selected_entity != entt::null && engine->reg.all_of<Transform>(selected_entity) && gizmo_enabled)
		{
			ImGuizmo::SetDrawlist();

			ImVec2 window_size = ImGui::GetWindowSize();
			ImVec2 window_pos = ImGui::GetWindowPos();

			ImGuizmo::SetRect(window_pos.x, window_pos.y,
				window_size.x, window_size.y);

			auto& camera = *engine->camera;

			Matrix camera_view = camera.View();
			Matrix camera_proj = camera.Proj();
			auto& entity_transform = engine->reg.get<Transform>(selected_entity);

			if (ImGuizmo::IsUsing())
			{
				Matrix tr = entity_transform.current_transform;
				bool change = ImGuizmo::Manipulate(camera_view.m[0], camera_proj.m[0], gizmo_op, ImGuizmo::LOCAL, tr.m[0]);
				entity_transform.current_transform = tr;
			}
		}

		ImGui::End();
	}
	void Editor::Log()
	{
		if (!visibility_flags[Flag_Log]) return;
		logger->Draw(ICON_FA_COMMENT" Log", &visibility_flags[Flag_Log]);
	}
	void Editor::Console()
	{
		if (show_basic_console)
		{
			ImGui::SetNextWindowSize(ImVec2(viewport_data.scene_viewport_size_x, 65));
			ImGui::SetNextWindowPos(ImVec2(viewport_data.scene_viewport_pos_x, viewport_data.scene_viewport_pos_y + viewport_data.scene_viewport_size_y - 65));
			console->DrawBasic(ICON_FA_TERMINAL "BasicConsole ", nullptr);
		}
		if (!visibility_flags[Flag_Console]) return;
		console->Draw(ICON_FA_TERMINAL "Console ", &visibility_flags[Flag_Console]);
	}

	void Editor::Settings()
	{
		if (!visibility_flags[Flag_Settings]) return;

		std::array<std::vector<GUICommand*>, GUICommandGroup_Count> grouped_commands;
		for (auto&& cmd : commands)
		{
			grouped_commands[cmd.group].push_back(&cmd);
		}

		if (ImGui::Begin(ICON_FA_GEAR" Settings", &visibility_flags[Flag_Settings]))
		{
			for (uint32 i = 0; i < GUICommandGroup_Count; ++i)
			{
				if (i != GUICommandGroup_None)
				{
					ImGui::SeparatorText(GUICommandGroupNames[i]);
				}
				std::array<std::vector<GUICommand*>, GUICommandSubGroup_Count> subgrouped_commands;
				for (auto&& cmd : grouped_commands[i])
				{
					subgrouped_commands[cmd->subgroup].push_back(cmd);
				}
				for (uint32 i = 0; i < GUICommandSubGroup_Count; ++i)
				{
					if (subgrouped_commands[i].empty()) continue;

					if (i == GUICommandSubGroup_None)
					{
						for (auto* cmd : subgrouped_commands[i]) cmd->callback();
					}
					else
					{
						if (ImGui::TreeNode(GUICommandSubGroupNames[i]))
						{
							for (auto* cmd : subgrouped_commands[i]) cmd->callback();
							ImGui::TreePop();
						}
					}
				}

			}
		}
		ImGui::End();
	}
	void Editor::Profiling()
	{
		if (!visibility_flags[Flag_Profiler]) return;
		if (ImGui::Begin(ICON_FA_CLOCK" Profiling", &visibility_flags[Flag_Profiler]))
		{
			ImGuiIO io = ImGui::GetIO();
#if GFX_PROFILING_USE_TRACY
			if (ImGui::Button("Run Tracy"))
			{
				system("start ..\\External\\tracy\\Tracy-0.9.1\\Tracy.exe");
			}
#endif
			static bool show_profiling = true;
			ImGui::Checkbox("Show Profiling Results", &show_profiling);
			if (show_profiling)
			{
				static constexpr uint64 NUM_FRAMES = 128;
				static constexpr int32 FRAME_TIME_GRAPH_MAX_FPS[] = { 800, 240, 120, 90, 65, 45, 30, 15, 10, 5, 4, 3, 2, 1 };

				static ProfilerState state{};
				static float FrameTimeArray[NUM_FRAMES] = { 0 };
				static float RecentHighestFrameTime = 0.0f;
				static float FrameTimeGraphMaxValues[ARRAYSIZE(FRAME_TIME_GRAPH_MAX_FPS)] = { 0 };
				for (uint64 i = 0; i < ARRAYSIZE(FrameTimeGraphMaxValues); ++i) { FrameTimeGraphMaxValues[i] = 1000.f / FRAME_TIME_GRAPH_MAX_FPS[i]; }

				std::vector<GfxTimestamp> time_stamps = g_GfxProfiler.GetResults();
				FrameTimeArray[NUM_FRAMES - 1] = 1000.0f / io.Framerate;
				for (uint32 i = 0; i < NUM_FRAMES - 1; i++) FrameTimeArray[i] = FrameTimeArray[i + 1];
				RecentHighestFrameTime = std::max(RecentHighestFrameTime, FrameTimeArray[NUM_FRAMES - 1]);

				float frame_time_ms = FrameTimeArray[NUM_FRAMES - 1];
				int32 const fps = static_cast<int32>(1000.0f / frame_time_ms);
				ImGui::Text("FPS        : %d (%.2f ms)", fps, frame_time_ms);
				if (ImGui::CollapsingHeader("Timings", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Show Avg/Min/Max", &state.show_average);
					ImGui::Spacing();

					uint64 max_i = 0;
					for (uint64 i = 0; i < ARRAYSIZE(FrameTimeGraphMaxValues); ++i)
					{
						if (RecentHighestFrameTime < FrameTimeGraphMaxValues[i])
						{
							max_i = std::min(ARRAYSIZE(FrameTimeGraphMaxValues) - 1, i + 1);
							break;
						}
					}
					ImGui::PlotLines("", FrameTimeArray, NUM_FRAMES, 0, "GPU frame time (ms)", 0.0f, FrameTimeGraphMaxValues[max_i], ImVec2(0, 80));

					constexpr uint32 avg_timestamp_update_interval = 1000;
					static auto MillisecondsNow = []()
					{
						static LARGE_INTEGER s_frequency;
						static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
						double milliseconds = 0;
						if (s_use_qpc)
						{
							LARGE_INTEGER now;
							QueryPerformanceCounter(&now);
							milliseconds = double(1000.0 * now.QuadPart) / s_frequency.QuadPart;
						}
						else milliseconds = double(GetTickCount64());
						return milliseconds;
					};
					const double current_time = MillisecondsNow();

					bool reset_accumulating_state = false;
					if ((state.accumulating_frame_count > 1) &&
						((current_time - state.last_reset_time) > avg_timestamp_update_interval))
					{
						std::swap(state.displayed_timestamps, state.accumulating_timestamps);
						for (uint32 i = 0; i < state.displayed_timestamps.size(); i++)
						{
							state.displayed_timestamps[i].sum /= state.accumulating_frame_count;
						}
						reset_accumulating_state = true;
					}

					reset_accumulating_state |= (state.accumulating_timestamps.size() != time_stamps.size());
					if (reset_accumulating_state)
					{
						state.accumulating_timestamps.resize(0);
						state.accumulating_timestamps.resize(time_stamps.size());
						state.last_reset_time = current_time;
						state.accumulating_frame_count = 0;
					}

					float total_time_ms = 0.0f;
					ImGui::BeginTable("Profiler", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg);
					ImGui::TableSetupColumn("Pass");
					ImGui::TableSetupColumn("Time");
					for (uint64 i = 0; i < time_stamps.size(); i++)
					{
						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", time_stamps[i].name.c_str());
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%.2f ms", time_stamps[i].time_in_ms);
						
						if (state.show_average)
						{
							if (state.displayed_timestamps.size() == time_stamps.size())
							{
								ImGui::SameLine();
								ImGui::Text("  avg: %.2f ms", state.displayed_timestamps[i].sum);
								ImGui::SameLine();	 
								ImGui::Text("  min: %.2f ms", state.displayed_timestamps[i].minimum);
								ImGui::SameLine();	 
								ImGui::Text("  max: %.2f ms", state.displayed_timestamps[i].maximum);
							}
						
							ProfilerState::AccumulatedTimeStamp* accumulating_timestamp = &state.accumulating_timestamps[i];
							accumulating_timestamp->sum += time_stamps[i].time_in_ms;
							accumulating_timestamp->minimum = std::min<float>(accumulating_timestamp->minimum, time_stamps[i].time_in_ms);
							accumulating_timestamp->maximum = std::max<float>(accumulating_timestamp->maximum, time_stamps[i].time_in_ms);
						}
						total_time_ms += time_stamps[i].time_in_ms;
					}
					ImGui::EndTable();
					ImGui::Text("Total: %7.2f %s", total_time_ms, "ms");
					state.accumulating_frame_count++;
				}
			}
			static bool display_vram_usage = false;
			ImGui::Checkbox("Display VRAM Usage", &display_vram_usage);
			if (display_vram_usage)
			{
				GPUMemoryUsage vram = gfx->GetMemoryUsage();
				float const ratio = vram.usage * 1.0f / vram.budget;
				std::string vram_display_string = "VRAM usage: " + std::to_string(vram.usage / 1024 / 1024) + "MB / " + std::to_string(vram.budget / 1024 / 1024) + "MB\n";
				if (ratio >= 0.9f && ratio <= 1.0f) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
				else if (ratio > 1.0f) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
				else ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
				ImGui::TextWrapped(vram_display_string.c_str());
				ImGui::PopStyleColor();
			}
		}
		ImGui::End();
	}
	void Editor::ShaderHotReload()
	{
		if (!visibility_flags[Flag_HotReload]) return;
		if (ImGui::Begin(ICON_FA_FIRE" Shader Hot Reload", &visibility_flags[Flag_HotReload]))
		{
			if (ImGui::Button("Compile Changed Shaders")) reload_shaders = true;
		}
		ImGui::End();
	}
	void Editor::Debug()
	{
		if (!visibility_flags[Flag_Debug]) return;
		if(ImGui::Begin(ICON_FA_BUG" Debug", &visibility_flags[Flag_Debug]))
		{
			if (ImGui::TreeNode("Debug Renderer"))
			{
				enum DebugRendererPrimitive
				{
					Line,
					Ray,
					Box,
					Sphere
				};
				static int current_debug_renderer_primitive = 0;
				static float debug_color[4] = { 0.0f,0.0f, 0.0f, 1.0f };
				ImGui::Combo("Debug Renderer Primitive", &current_debug_renderer_primitive, "Line\0Ray\0Box\0Sphere\0", 4);
				ImGui::ColorEdit3("Debug Color", debug_color);

				g_DebugRenderer.SetMode(DebugRendererMode::Persistent);
				switch (current_debug_renderer_primitive)
				{
				case Line:
				{
					static float start[3] = { 0.0f };
					static float end[3] = { 0.0f };
					ImGui::InputFloat3("Line Start", start);
					ImGui::InputFloat3("Line End", end);
					if (ImGui::Button("Add")) g_DebugRenderer.AddLine(Vector3(start), Vector3(end), Color(debug_color));
				}
				break;
				case Ray:
				{
					static float origin[3] = { 0.0f };
					static float dir[3] = { 0.0f };
					ImGui::InputFloat3("Ray Origin", origin);
					ImGui::InputFloat3("Ray Direction", dir);
					if (ImGui::Button("Add")) g_DebugRenderer.AddRay(Vector3(origin), Vector3(dir), Color(debug_color));
				}
				break;
				case Box:
				{
					static float center[3] = { 0.0f };
					static float extents[3] = { 0.0f };
					static bool wireframe = false;
					ImGui::InputFloat3("Box Center", center);
					ImGui::InputFloat3("Box Extents", extents);
					ImGui::Checkbox("Wireframe", &wireframe);
					if (ImGui::Button("Add")) g_DebugRenderer.AddBox(Vector3(center), Vector3(extents), Color(debug_color), wireframe);
				}
				break;
				case Sphere:
				{
					static float center[3] = { 0.0f };
					static float radius = 1.0f;
					static bool wireframe = false;
					ImGui::InputFloat3("Sphere Center", center);
					ImGui::InputFloat("Sphere Radius", &radius);
					ImGui::Checkbox("Wireframe", &wireframe);
					if (ImGui::Button("Add")) g_DebugRenderer.AddSphere(Vector3(center), radius, Color(debug_color), wireframe);
				}
				break;
				}
				g_DebugRenderer.SetMode(DebugRendererMode::Transient);

				if (ImGui::Button("Clear")) g_DebugRenderer.ClearPersistent();
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Render Graph"))
			{
				dump_render_graph = ImGui::Button("Dump render graph");
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Screenshot"))
			{
				static char filename[32] = "screenshot";
				ImGui::InputText("File name", filename, sizeof(filename));
				if (ImGui::Button("Take Screenshot"))
				{
					editor_events.take_screenshot_event.Broadcast(filename);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("PIX"))
			{
				static char capture_name[32] = { 'a', 'd', 'r', 'i', 'a' };
				ImGui::InputText("Capture name", capture_name, sizeof(capture_name));

				static int frame_count = 1;
				ImGui::SliderInt("Number of capture frames", &frame_count, 1, 10);

				if (ImGui::Button("Take capture"))
				{
					std::string capture_full_path = paths::PixCapturesDir + capture_name;
					gfx->TakePixCapture(capture_full_path.c_str(), frame_count);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Textures"))
			{
				struct VoidPointerHash
				{
					uint64 operator()(void const* ptr) const { return reinterpret_cast<uint64>(ptr); }
				};
				static std::unordered_map<void const*, GfxDescriptor, VoidPointerHash> debug_srv_map;

				for (int32 i = 0; i < debug_textures.size(); ++i)
				{
					ImGui::PushID(i);
					auto& debug_texture = debug_textures[i];
					ImGui::Text(debug_texture.name);
					GfxDescriptor debug_srv_gpu = gui->AllocateDescriptorsGPU();
					if (debug_srv_map.contains(debug_texture.gfx_texture))
					{
						GfxDescriptor debug_srv_cpu = debug_srv_map[debug_texture.gfx_texture];
						gfx->CopyDescriptors(1, debug_srv_gpu, debug_srv_cpu);
					}
					else
					{
						GfxDescriptor debug_srv_cpu = gfx->CreateTextureSRV(debug_texture.gfx_texture);
						debug_srv_map[debug_texture.gfx_texture] = debug_srv_cpu;
						gfx->CopyDescriptors(1, debug_srv_gpu, debug_srv_cpu);
					}
					uint32 width = debug_texture.gfx_texture->GetDesc().width;
					uint32 height = debug_texture.gfx_texture->GetDesc().height;
					float window_width = ImGui::GetWindowWidth();
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(debug_srv_gpu).ptr, ImVec2(window_width * 0.9f, window_width * 0.9f * (float)height / width));
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}
}
