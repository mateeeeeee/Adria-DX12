#include "nfd.h"
#include "Editor.h"
#include "GUI.h"
#include "EditorUtil.h"
#include "Core/Engine.h"
#include "Input/Input.h"
#include "IconsFontAwesome4.h"
#include "Rendering/Renderer.h"
#include "Rendering/Camera.h"
#include "Rendering/EntityLoader.h"
#include "Rendering/ShaderCache.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxProfiler.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/FilesUtil.h"
#include "Utilities/StringUtil.h"
#include "Utilities/Random.h"
#include "Core/ConsoleVariable.h"
#include "Core/ConsoleCommand.h"
#include "Math/BoundingVolumeHelpers.h"


using namespace DirectX;


namespace adria
{
	bool dump_render_graph = false;

	namespace cvars
	{
		static ConsoleVariable ao_cvar("ao", 1);
		static ConsoleVariable reflections("reflections", 0);
		static ConsoleVariable renderpath("renderpath", 0);
		static ConsoleVariable taa("TAA", false);
		static ConsoleVariable fxaa("FXAA", true);
		static ConsoleVariable exposure("exposure", true);
		static ConsoleVariable clouds("clouds", true);
		static ConsoleVariable dof("dof", false);
		static ConsoleVariable bokeh("bokeh", false);
		static ConsoleVariable bloom("bloom", true);
		static ConsoleVariable motion_blur("motionblur", false);
		static ConsoleVariable fog("fog", false);

		ConsoleCommand<> dump_render_graph_cmd("dump.rendergraph", []() { dump_render_graph = true; });
	}

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
		logger = std::make_unique<EditorLogger>();
		console = std::make_unique<EditorConsole>();

		engine = std::make_unique<Engine>(init.engine_init);
		gfx = engine->gfx.get();
		gui = std::make_unique<GUI>(gfx);
		engine->RegisterEditorEventCallbacks(editor_events);

		ray_tracing_supported = gfx->GetCapabilities().SupportsRayTracing();
		selected_entity = entt::null;
		SetStyle();
	}
	void Editor::Destroy()
	{
		gui.reset();
		engine.reset();
		console.reset();
		logger.reset();
	}
	void Editor::HandleWindowMessage(WindowMessage const& msg_data)
	{
		engine->HandleWindowMessage(msg_data);
		gui->HandleWindowMessage(msg_data);
	}
	void Editor::Run()
	{
		HandleInput();
		if (gui->IsVisible()) engine->SetViewportData(viewport_data);
		else engine->SetViewportData(std::nullopt);

		engine->Run(renderer_settings);
		engine->Present();

		if (reload_shaders)
		{
			gfx->WaitForGPU();
			ShaderCache::CheckIfShadersHaveChanged();
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
				data.src = builder.ReadTexture(RG_RES_NAME(FinalTexture));
				data.rt = builder.WriteRenderTarget(RG_RES_NAME(Backbuffer), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(engine->renderer->GetWidth(), engine->renderer->GetHeight());
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

		style.FrameRounding = 0.0f;
		style.GrabRounding = 1.0f;
		style.WindowRounding = 0.0f;
		style.IndentSpacing = 10.0f;
		style.ScrollbarSize = 16.0f;
		style.WindowPadding = ImVec2(5, 5);
		style.FramePadding = ImVec2(2, 2);

		ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
		colors[ImGuiCol_Border] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.14f, 0.71f, 0.83f, 0.95f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.67f, 0.82f, 0.83f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.42f, 0.80f, 0.96f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 0.58f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.23f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);
		colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.37f, 0.37f, 0.37f, 0.80f);
		colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.73f, 0.29f, 0.29f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	}
	void Editor::HandleInput()
	{
		if (g_Input.IsKeyDown(KeyCode::I)) gui->ToggleVisibility();
		if (scene_focused && g_Input.IsKeyDown(KeyCode::G)) gizmo_enabled = !gizmo_enabled;
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
				if (ImGui::MenuItem(ICON_FA_CLOCK_O" Profiler", 0, window_flags[Flag_Profiler]))			 window_flags[Flag_Profiler] = !window_flags[Flag_Profiler];
				if (ImGui::MenuItem(ICON_FA_COMMENT" Log", 0, window_flags[Flag_Log]))						 window_flags[Flag_Log] = !window_flags[Flag_Log];
				if (ImGui::MenuItem("Console", 0, window_flags[Flag_Console]))				 window_flags[Flag_Console] = !window_flags[Flag_Console];
				if (ImGui::MenuItem(ICON_FA_CAMERA" Camera", 0, window_flags[Flag_Camera]))				 window_flags[Flag_Camera] = !window_flags[Flag_Camera];
				if (ImGui::MenuItem("Entities", 0, window_flags[Flag_Entities]))			 window_flags[Flag_Entities] = !window_flags[Flag_Entities];
				if (ImGui::MenuItem(ICON_FA_FIRE" Hot Reload", 0, window_flags[Flag_HotReload]))			 window_flags[Flag_HotReload] = !window_flags[Flag_HotReload];
				if (ImGui::MenuItem(ICON_FA_COGS" Settings", 0, window_flags[Flag_Settings]))			 window_flags[Flag_Settings] = !window_flags[Flag_Settings];
				if (ImGui::MenuItem(ICON_FA_BUG" Debug", 0, window_flags[Flag_Debug]))					 window_flags[Flag_Debug] = !window_flags[Flag_Debug];
				if (ImGui::MenuItem(ICON_FA_PLUS "Add Entities", 0, window_flags[Flag_AddEntities]))		 window_flags[Flag_AddEntities] = !window_flags[Flag_AddEntities];

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
		if (!window_flags[Flag_AddEntities]) return;
		if (ImGui::Begin("Add Entities", &window_flags[Flag_AddEntities]))
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
						light_params.light_data.color = XMVectorSet(real() * 2, real() * 2, real() * 2, 1.0f);
						light_params.light_data.direction = XMVectorSet(0.5f, -1.0f, 0.1f, 0.0f);
						light_params.light_data.position = XMVectorSet(real() * 200 - 100, real() * 200.0f, real() * 200 - 100, 1.0f);
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
						light_params.light_data.color = XMVectorSet(real() * 2, real() * 2, real() * 2, 1.0f);
						light_params.light_data.direction = XMVectorSet(0.5f, -1.0f, 0.1f, 0.0f);
						light_params.light_data.position = XMVectorSet(real() * 200 - 100, real() * 200.0f, real() * 200 - 100, 1.0f);
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

				auto picking_data = engine->renderer->GetPickingData();
				ImGui::Text("Picked Position: %f %f %f", picking_data.position.x, picking_data.position.y, picking_data.position.z);
				ImGui::Text("Picked Normal: %f %f %f", picking_data.normal.x, picking_data.normal.y, picking_data.normal.z);
				if (ImGui::Button("Load Decal"))
				{
					params.position = picking_data.position;
					params.normal = picking_data.normal;
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
		if (!window_flags[Flag_Entities]) return;
		auto all_entities = engine->reg.view<Tag>();
		if (ImGui::Begin("Entities", &window_flags[Flag_Entities]))
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
		if (!window_flags[Flag_Entities]) return;
		if (ImGui::Begin("Properties", &window_flags[Flag_Entities]))
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

					//Vector4 light_color, light_direction, light_position;
					//XMStoreFloat4(&light_color, light->color);
					//XMStoreFloat4(&light_direction, light->direction);
					//XMStoreFloat4(&light_position, light->position);

					float color[3] = { light->color.x, light->color.y, light->color.z };
					ImGui::ColorEdit3("Light Color", color);
					light->color = Vector4(color[0], color[1], color[2], 1.0f);

					ImGui::SliderFloat("Light Energy", &light->energy, 0.0f, 50.0f);

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
						const char* shadow_types[] = { "None", "Shadow Maps", "Ray Traced Shadows" };
						static int current_shadow_type = light->casts_shadows;
						const char* combo_label = shadow_types[current_shadow_type];
						if (ImGui::BeginCombo("Shadows Type", combo_label, 0))
						{
							for (int n = 0; n < IM_ARRAYSIZE(shadow_types); n++)
							{
								const bool is_selected = (current_shadow_type == n);
								if (ImGui::Selectable(shadow_types[n], is_selected)) current_shadow_type = n;
								if (is_selected) ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						if (!ray_tracing_supported && current_shadow_type == 2)
						{
							current_shadow_type = 1;
						}
						light->casts_shadows = (current_shadow_type == 1);
						light->ray_traced_shadows = (current_shadow_type == 2);
					}
					else
					{
						ImGui::Checkbox("Casts Shadows", &light->casts_shadows);
					}

					if (light->casts_shadows)
					{
						if (light->type == LightType::Directional && light->casts_shadows)
						{
							bool use_cascades = static_cast<bool>(light->use_cascades);
							ImGui::Checkbox("Use Cascades", &use_cascades);
							light->use_cascades = use_cascades;
						}
						ImGui::Checkbox("Screen Space Contact Shadows", &light->sscs);
						if (light->sscs)
						{
							ImGui::SliderFloat("Thickness", &light->sscs_thickness, 0.0f, 1.0f);
							ImGui::SliderFloat("Max Ray Distance", &light->sscs_max_ray_distance, 0.0f, 0.3f);
							ImGui::SliderFloat("Max Depth Distance", &light->sscs_max_depth_distance, 0.0f, 500.0f);
						}
					}
					else if (light->ray_traced_shadows)
					{
						ImGui::Checkbox("Soft Shadows", &light->soft_rts);
					}

					ImGui::Checkbox("God Rays", &light->god_rays);
					if (light->god_rays)
					{
						ImGui::SliderFloat("God Rays decay", &light->godrays_decay, 0.0f, 1.0f);
						ImGui::SliderFloat("God Rays weight", &light->godrays_weight, 0.0f, 1.0f);
						ImGui::SliderFloat("God Rays density", &light->godrays_density, 0.1f, 2.0f);
						ImGui::SliderFloat("God Rays exposure", &light->godrays_exposure, 0.1f, 10.0f);
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
					GfxDescriptor tex_handle = g_TextureManager.GetSRV(material->albedo_texture);
					GfxDescriptor dst_descriptor = gui->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, ImVec2(48.0f, 48.0f));

					ImGui::PushID(0);
					if (ImGui::Button("Remove")) material->albedo_texture = INVALID_TEXTURE_HANDLE;
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
					tex_handle = g_TextureManager.GetSRV(material->metallic_roughness_texture);
					dst_descriptor = gui->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(1);
					if (ImGui::Button("Remove")) material->metallic_roughness_texture = INVALID_TEXTURE_HANDLE;
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
					tex_handle = g_TextureManager.GetSRV(material->emissive_texture);
					dst_descriptor = gui->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(2);
					if (ImGui::Button("Remove")) material->emissive_texture = INVALID_TEXTURE_HANDLE;
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
		if (!window_flags[Flag_Camera]) return;

		auto& camera = *engine->camera;
		if (ImGui::Begin(ICON_FA_CAMERA" Camera", &window_flags[Flag_Camera]))
		{
			Vector3 cam_pos = camera.Position();
			float pos[3] = { cam_pos.x , cam_pos.y, cam_pos.z };
			ImGui::SliderFloat3("Position", pos, 0.0f, 2000.0f);
			camera.SetPosition(Vector3(pos));
			float near_plane = camera.Near(), far_plane = camera.Far();
			float fov = camera.Fov(), ar = camera.AspectRatio();
			ImGui::SliderFloat("Near", &near_plane, 0.0f, 2.0f);
			ImGui::SliderFloat("Far", &far_plane, 10.0f, 3000.0f);
			ImGui::SliderFloat("FOV", &fov, 0.01f, 1.5707f);
			camera.SetNearAndFar(near_plane, far_plane);
			camera.SetFov(fov);
		}
		ImGui::End();
	}
	void Editor::Scene(GfxDescriptor& src)
	{
		ImGui::Begin("Scene");
		{
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

			auto camera_view = camera.View();
			auto camera_proj = camera.Proj();

			XMFLOAT4X4 view, projection;

			XMStoreFloat4x4(&view, camera_view);
			XMStoreFloat4x4(&projection, camera_proj);

			auto& entity_transform = engine->reg.get<Transform>(selected_entity);

			XMFLOAT4X4 tr;
			XMStoreFloat4x4(&tr, entity_transform.current_transform);

			if (ImGuizmo::IsUsing())
			{
				bool change = ImGuizmo::Manipulate(view.m[0], projection.m[0], gizmo_op, ImGuizmo::LOCAL, tr.m[0]);
				entity_transform.current_transform = XMLoadFloat4x4(&tr);
			}
		}

		ImGui::End();
	}
	void Editor::Log()
	{
		if (!window_flags[Flag_Log]) return;
		if(ImGui::Begin(ICON_FA_COMMENT" Log", &window_flags[Flag_Log]))
		{
			logger->Draw(ICON_FA_COMMENT" Log");
		}
		ImGui::End();
	}
	void Editor::Console()
	{
		if (!window_flags[Flag_Console]) return;
		if (ImGui::Begin("Console", &window_flags[Flag_Console]))
		{
			console->Draw("Console");
		}
		ImGui::End();
	}

	void Editor::Settings()
	{
		int& current_render_path_type = cvars::renderpath.Get();
		int& current_ao_type = cvars::ao_cvar.Get();
		int& current_reflection_type = cvars::reflections.Get();

		renderer_settings.render_path = static_cast<RenderPathType>(current_render_path_type);
		renderer_settings.postprocess.ambient_occlusion = static_cast<AmbientOcclusion>(current_ao_type);
		renderer_settings.postprocess.reflections = static_cast<Reflections>(current_reflection_type);
		renderer_settings.postprocess.automatic_exposure = cvars::exposure;
		renderer_settings.postprocess.clouds = cvars::clouds;
		renderer_settings.postprocess.dof = cvars::dof;
		renderer_settings.postprocess.bokeh = cvars::bokeh;
		renderer_settings.postprocess.bloom = cvars::bloom;
		renderer_settings.postprocess.motion_blur = cvars::motion_blur;
		renderer_settings.postprocess.fog = cvars::fog;

		if (!window_flags[Flag_Settings]) return;
		if (ImGui::Begin(ICON_FA_COGS" Settings", &window_flags[Flag_Settings]))
		{
			static const char* render_path_types[] = { "Regular Deferred", "Tiled Deferred", "Clustered Deferred", "Path Tracing" };
			const char* render_path_combo_label = render_path_types[current_render_path_type];
			if (ImGui::BeginCombo("Render Path", render_path_combo_label, 0))
			{
				for (int n = 0; n < IM_ARRAYSIZE(render_path_types); n++)
				{
					const bool is_selected = (current_render_path_type == n);
					if (ImGui::Selectable(render_path_types[n], is_selected)) current_render_path_type = n;
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (!ray_tracing_supported && current_render_path_type == 3)
			{
				current_render_path_type = 0;
			}

			static const char* ao_types[] = { "None", "SSAO", "HBAO", "RTAO" };
			const char* ao_combo_label = ao_types[current_ao_type];
			if (ImGui::BeginCombo("Ambient Occlusion", ao_combo_label, 0))
			{
				for (int n = 0; n < IM_ARRAYSIZE(ao_types); n++)
				{
					const bool is_selected = (current_ao_type == n);
					if (ImGui::Selectable(ao_types[n], is_selected)) current_ao_type = n;
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (!ray_tracing_supported && current_ao_type == 3)
			{
				current_ao_type = 1;
			}

			static const char* reflection_types[] = { "None", "SSR", "RTR" };
			const char* reflection_combo_label = reflection_types[current_reflection_type];
			if (ImGui::BeginCombo("Reflections", reflection_combo_label, 0))
			{
				for (int n = 0; n < IM_ARRAYSIZE(reflection_types); n++)
				{
					const bool is_selected = (current_reflection_type == n);
					if (ImGui::Selectable(reflection_types[n], is_selected)) current_reflection_type = n;
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (!ray_tracing_supported && current_reflection_type == 2)
			{
				current_reflection_type = 1;
			}

			ImGui::Checkbox("Automatic Exposure", &cvars::exposure.Get());
			ImGui::Checkbox("Volumetric Clouds", &cvars::clouds.Get());
			ImGui::Checkbox("DoF", &cvars::dof.Get());
			if (cvars::dof)
			{
				ImGui::Checkbox("Bokeh", &cvars::bokeh.Get());
			}
			ImGui::Checkbox("Bloom", &cvars::bloom.Get());
			ImGui::Checkbox("Motion Blur", &cvars::motion_blur.Get());
			ImGui::Checkbox("Fog", &cvars::fog.Get());
			if (ImGui::TreeNode("Anti-Aliasing"))
			{
				bool& fxaa = cvars::fxaa.Get();
				bool& taa  = cvars::taa.Get();
				ImGui::Checkbox("FXAA", &fxaa);
				ImGui::Checkbox("TAA", &taa);
				if (fxaa) renderer_settings.postprocess.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.postprocess.anti_aliasing | AntiAliasing_FXAA);
				else renderer_settings.postprocess.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.postprocess.anti_aliasing & (~AntiAliasing_FXAA));

				if (taa) renderer_settings.postprocess.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.postprocess.anti_aliasing | AntiAliasing_TAA);
				else renderer_settings.postprocess.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.postprocess.anti_aliasing & (~AntiAliasing_TAA));

				ImGui::TreePop();
			}

			for (auto&& cmd : commands) cmd.callback();
		}
		ImGui::End();
	}
	void Editor::Profiling()
	{
		if (!window_flags[Flag_Profiler]) return;
		if (ImGui::Begin(ICON_FA_CLOCK_O" Profiling", &window_flags[Flag_Profiler]))
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
				static ProfilerState state;
				static constexpr uint64 NUM_FRAMES = 128;
				static float FRAME_TIME_ARRAY[NUM_FRAMES] = { 0 };
				static float RECENT_HIGHEST_FRAME_TIME = 0.0f;
				static constexpr int32 FRAME_TIME_GRAPH_MAX_FPS[] = { 800, 240, 120, 90, 65, 45, 30, 15, 10, 5, 4, 3, 2, 1 };
				static float FRAME_TIME_GRAPH_MAX_VALUES[ARRAYSIZE(FRAME_TIME_GRAPH_MAX_FPS)] = { 0 };
				for (uint64 i = 0; i < ARRAYSIZE(FRAME_TIME_GRAPH_MAX_FPS); ++i) { FRAME_TIME_GRAPH_MAX_VALUES[i] = 1000.f / FRAME_TIME_GRAPH_MAX_FPS[i]; }

				std::vector<GfxTimestamp> time_stamps = g_GfxProfiler.GetResults();
				FRAME_TIME_ARRAY[NUM_FRAMES - 1] = 1000.0f / io.Framerate;
				for (uint32 i = 0; i < NUM_FRAMES - 1; i++) FRAME_TIME_ARRAY[i] = FRAME_TIME_ARRAY[i + 1];
				RECENT_HIGHEST_FRAME_TIME = std::max(RECENT_HIGHEST_FRAME_TIME, FRAME_TIME_ARRAY[NUM_FRAMES - 1]);
				float frame_time_ms = FRAME_TIME_ARRAY[NUM_FRAMES - 1];
				const int32 fps = static_cast<int32>(1000.0f / frame_time_ms);

				ImGui::Text("FPS        : %d (%.2f ms)", fps, frame_time_ms);
				if (ImGui::CollapsingHeader("Timings", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Show Avg/Min/Max", &state.show_average);
					ImGui::Spacing();

					uint64 i_max = 0;
					for (uint64 i = 0; i < ARRAYSIZE(FRAME_TIME_GRAPH_MAX_VALUES); ++i)
					{
						if (RECENT_HIGHEST_FRAME_TIME < FRAME_TIME_GRAPH_MAX_VALUES[i]) // FRAME_TIME_GRAPH_MAX_VALUES are in increasing order
						{
							i_max = std::min(ARRAYSIZE(FRAME_TIME_GRAPH_MAX_VALUES) - 1, i + 1);
							break;
						}
					}
					ImGui::PlotLines("", FRAME_TIME_ARRAY, NUM_FRAMES, 0, "GPU frame time (ms)", 0.0f, FRAME_TIME_GRAPH_MAX_VALUES[i_max], ImVec2(0, 80));

					constexpr uint32_t avg_timestamp_update_interval = 1000;
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
					for (uint64 i = 0; i < time_stamps.size(); i++)
					{
						float value = time_stamps[i].time_in_ms;
						char const* pStrUnit = "ms";
						ImGui::Text("%-18s: %7.2f %s", time_stamps[i].name.c_str(), value, pStrUnit);
						if (state.show_average)
						{
							if (state.displayed_timestamps.size() == time_stamps.size())
							{
								ImGui::SameLine();
								ImGui::Text("  avg: %7.2f %s", state.displayed_timestamps[i].sum, pStrUnit);
								ImGui::SameLine();
								ImGui::Text("  min: %7.2f %s", state.displayed_timestamps[i].minimum, pStrUnit);
								ImGui::SameLine();
								ImGui::Text("  max: %7.2f %s", state.displayed_timestamps[i].maximum, pStrUnit);
							}

							ProfilerState::AccumulatedTimeStamp* pAccumulatingTimeStamp = &state.accumulating_timestamps[i];
							pAccumulatingTimeStamp->sum += time_stamps[i].time_in_ms;
							pAccumulatingTimeStamp->minimum = std::min<float>(pAccumulatingTimeStamp->minimum, time_stamps[i].time_in_ms);
							pAccumulatingTimeStamp->maximum = std::max<float>(pAccumulatingTimeStamp->maximum, time_stamps[i].time_in_ms);
						}
						total_time_ms += value;
					}
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
		if (!window_flags[Flag_HotReload]) return;
		if (ImGui::Begin(ICON_FA_FIRE" Shader Hot Reload", &window_flags[Flag_HotReload]))
		{
			if (ImGui::Button("Compile Changed Shaders")) reload_shaders = true;
		}
		ImGui::End();
	}
	void Editor::Debug()
	{
		if (!window_flags[Flag_Debug]) return;
		if(ImGui::Begin(ICON_FA_BUG" Debug", &window_flags[Flag_Debug]))
		{
			if (ImGui::TreeNode("Render Graph"))
			{
				dump_render_graph = ImGui::Button("Dump render graph");
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("PIX"))
			{
				static int frame_count = 1;
				ImGui::SliderInt("Number of capture frames", &frame_count, 1, 10);
				if (ImGui::Button("Take capture"))
				{
					gfx->TakePixCapture(frame_count);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Textures"))
			{
				for (int32 i = 0; i < debug_textures.size(); ++i)
				{
					ImGui::PushID(i);
					auto& debug_texture = debug_textures[i];
					ImGui::Text(debug_texture.name);
					GfxDescriptor tex_handle = gfx->CreateTextureSRV(debug_texture.gfx_texture);
					GfxDescriptor dst_descriptor = gui->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, tex_handle);
					uint32 width = debug_texture.gfx_texture->GetDesc().width;
					uint32 height = debug_texture.gfx_texture->GetDesc().width;
					float window_width = ImGui::GetWindowWidth();
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, ImVec2(window_width * 0.9f, window_width * 0.9f * (float)height / width));
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}
}
