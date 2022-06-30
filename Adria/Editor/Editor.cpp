#include "Editor.h"

#include "../Rendering/Renderer.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Rendering/ModelImporter.h"
#include "../Rendering/PipelineState.h"
#include "../Rendering/ShaderManager.h"
#include "../Logging/Logger.h"
#include "../Utilities/FilesUtil.h"
#include "../Utilities/StringUtil.h"
#include "../ImGui/ImGuiFileDialog.h"
#include "../Utilities/Random.h"
#include "../Math/BoundingVolumeHelpers.h"
#include "pix3.h"

using namespace DirectX;

namespace adria
{
	//heavily based of AMD Cauldron code
	struct ProfilerState
	{
		bool  show_average;
		struct AccumulatedTimeStamp
		{
			float sum;
			float minimum;
			float maximum;

			AccumulatedTimeStamp()
				: sum(0.0f), minimum(FLT_MAX), maximum(0)
			{
			}
		};

		std::vector<AccumulatedTimeStamp> displayed_timestamps;
		std::vector<AccumulatedTimeStamp> accumulating_timestamps;
		float64 last_reset_time;
		uint32 accumulating_frame_count;
	};

	enum class EMaterialTextureType
	{
		eAlbedo,
		eMetallicRoughness,
		eEmissive
	};

	struct ImGuiLogger
	{
		ImGuiTextBuffer     Buf;
		ImGuiTextFilter     Filter;
		ImVector<int>       LineOffsets;
		bool                AutoScroll;

		ImGuiLogger()
		{
			AutoScroll = true;
			Clear();
		}

		void Clear()
		{
			Buf.clear();
			LineOffsets.clear();
			LineOffsets.push_back(0);
		}

		void AddLog(const char* fmt, ...) IM_FMTARGS(2)
		{
			int old_size = Buf.size();
			va_list args;
			va_start(args, fmt);
			Buf.appendfv(fmt, args);
			va_end(args);
			for (int new_size = Buf.size(); old_size < new_size; old_size++)
				if (Buf[old_size] == '\n')
					LineOffsets.push_back(old_size + 1);
		}

		void Draw(const char* title, bool* p_open = NULL)
		{
			if (!ImGui::Begin(title, p_open))
			{
				ImGui::End();
				return;
			}

			// Options menu
			if (ImGui::BeginPopup("Options"))
			{
				ImGui::Checkbox("Auto-scroll", &AutoScroll);
				ImGui::EndPopup();
			}

			// Main window
			if (ImGui::Button("Options"))
				ImGui::OpenPopup("Options");
			ImGui::SameLine();
			bool clear = ImGui::Button("Clear");
			ImGui::SameLine();
			bool copy = ImGui::Button("Copy");
			ImGui::SameLine();
			Filter.Draw("Filter", -100.0f);

			ImGui::Separator();
			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

			if (clear)
				Clear();
			if (copy)
				ImGui::LogToClipboard();

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			const char* buf = Buf.begin();
			const char* buf_end = Buf.end();
			if (Filter.IsActive())
			{
				for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
				{
					const char* line_start = buf + LineOffsets[line_no];
					const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
					if (Filter.PassFilter(line_start, line_end))
						ImGui::TextUnformatted(line_start, line_end);
				}
			}
			else
			{
				ImGuiListClipper clipper;
				clipper.Begin(LineOffsets.Size);
				while (clipper.Step())
				{
					for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
					{
						const char* line_start = buf + LineOffsets[line_no];
						const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
						ImGui::TextUnformatted(line_start, line_end);
					}
				}
				clipper.End();
			}
			ImGui::PopStyleVar();

			if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);

			ImGui::EndChild();
			ImGui::End();
		}
	};

	class EditorLogger : public ILogger
	{
	public:
		EditorLogger(ImGuiLogger* logger, ELogLevel logger_level = ELogLevel::LOG_DEBUG) : logger{ logger }, logger_level{ logger_level } {}

		virtual void Log(ELogLevel level, char const* entry, char const* file, uint32_t line) override
		{
			if (level < logger_level) return;
			std::string log_entry = GetLogTime() + LevelToString(level) + std::string(entry) + "\n";
			if (logger) logger->AddLog(log_entry.c_str());
		}
	private:
		ImGuiLogger* logger;
		ELogLevel logger_level;
	};

	Editor::Editor(EditorInit const& init) : engine(), editor_log(new ImGuiLogger{})
	{
		engine = std::make_unique<Engine>(init.engine_init);
		gui = std::make_unique<GUI>(engine->gfx.get());
		ADRIA_REGISTER_LOGGER(new EditorLogger(editor_log.get()));
		engine->RegisterEditorEventCallbacks(editor_events);
		SetStyle();
	}

	Editor::~Editor() = default;

	void Editor::HandleWindowMessage(WindowMessage const& msg_data)
	{
		engine->HandleWindowMessage(msg_data);
		gui->HandleWindowMessage(msg_data);
	}

	void Editor::Run()
	{
		HandleInput();
		renderer_settings.gui_visible = gui->IsVisible();
		if (gui->IsVisible())
		{
			engine->SetViewportData(viewport_data);
			engine->Run(renderer_settings);
			auto gui_cmd_list = engine->gfx->GetDefaultCommandList();
			engine->gfx->SetBackbuffer(gui_cmd_list);
			{
				PIXScopedEvent(gui_cmd_list, PIX_COLOR_DEFAULT, "GUI Pass");
				gui->Begin();
				MenuBar();
				ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
				ListEntities();
				OceanSettings();
				ParticleSettings();
				DecalSettings();
				SkySettings();
				AddEntities();
				Camera();
				Scene();
				RendererSettings();
				Properties();
				Log();
				Profiling();
				ShaderHotReload();
				if (engine->renderer->IsRayTracingSupported())
					RayTracingDebug();
				gui->End(gui_cmd_list);
			}
			if (!aabb_updates.empty())
			{
				engine->gfx->WaitForGPU();
				while (!aabb_updates.empty())
				{
					AABB* aabb = aabb_updates.front();
					aabb->UpdateBuffer(engine->gfx.get());
					aabb_updates.pop();
				}
			}
			engine->Present();
		}
		else
		{
			engine->SetViewportData(std::nullopt);
			engine->Run(renderer_settings);
			engine->Present();
		}


		if (reload_shaders)
		{
			engine->gfx->WaitForGPU();
			ShaderManager::CheckIfShadersHaveChanged();
		}
	}

	void Editor::SetStyle()
	{
		constexpr auto ColorFromBytes = [](uint8_t r, uint8_t g, uint8_t b)
		{
			return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f);
		};

		auto& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		const ImVec4 bgColor = ColorFromBytes(37, 37, 38);
		const ImVec4 lightBgColor = ColorFromBytes(82, 82, 85);
		const ImVec4 veryLightBgColor = ColorFromBytes(90, 90, 95);

		const ImVec4 panelColor = ColorFromBytes(51, 51, 55);
		const ImVec4 panelHoverColor = ColorFromBytes(29, 151, 236);
		const ImVec4 panelActiveColor = ColorFromBytes(0, 119, 200);

		const ImVec4 textColor = ColorFromBytes(255, 255, 255);
		const ImVec4 textDisabledColor = ColorFromBytes(151, 151, 151);
		const ImVec4 borderColor = ColorFromBytes(78, 78, 78);

		colors[ImGuiCol_Text] = textColor;
		colors[ImGuiCol_TextDisabled] = textDisabledColor;
		colors[ImGuiCol_TextSelectedBg] = panelActiveColor;
		colors[ImGuiCol_WindowBg] = bgColor;
		colors[ImGuiCol_ChildBg] = bgColor;
		colors[ImGuiCol_PopupBg] = bgColor;
		colors[ImGuiCol_Border] = borderColor;
		colors[ImGuiCol_BorderShadow] = borderColor;
		colors[ImGuiCol_FrameBg] = panelColor;
		colors[ImGuiCol_FrameBgHovered] = panelHoverColor;
		colors[ImGuiCol_FrameBgActive] = panelActiveColor;
		colors[ImGuiCol_TitleBg] = bgColor;
		colors[ImGuiCol_TitleBgActive] = bgColor;
		colors[ImGuiCol_TitleBgCollapsed] = bgColor;
		colors[ImGuiCol_MenuBarBg] = panelColor;
		colors[ImGuiCol_ScrollbarBg] = panelColor;
		colors[ImGuiCol_ScrollbarGrab] = lightBgColor;
		colors[ImGuiCol_ScrollbarGrabHovered] = veryLightBgColor;
		colors[ImGuiCol_ScrollbarGrabActive] = veryLightBgColor;
		colors[ImGuiCol_CheckMark] = panelActiveColor;
		colors[ImGuiCol_SliderGrab] = panelHoverColor;
		colors[ImGuiCol_SliderGrabActive] = panelActiveColor;
		colors[ImGuiCol_Button] = panelColor;
		colors[ImGuiCol_ButtonHovered] = panelHoverColor;
		colors[ImGuiCol_ButtonActive] = panelHoverColor;
		colors[ImGuiCol_Header] = panelColor;
		colors[ImGuiCol_HeaderHovered] = panelHoverColor;
		colors[ImGuiCol_HeaderActive] = panelActiveColor;
		colors[ImGuiCol_Separator] = borderColor;
		colors[ImGuiCol_SeparatorHovered] = borderColor;
		colors[ImGuiCol_SeparatorActive] = borderColor;
		colors[ImGuiCol_ResizeGrip] = bgColor;
		colors[ImGuiCol_ResizeGripHovered] = panelColor;
		colors[ImGuiCol_ResizeGripActive] = lightBgColor;
		colors[ImGuiCol_PlotLines] = panelActiveColor;
		colors[ImGuiCol_PlotLinesHovered] = panelHoverColor;
		colors[ImGuiCol_PlotHistogram] = panelActiveColor;
		colors[ImGuiCol_PlotHistogramHovered] = panelHoverColor;
		colors[ImGuiCol_DragDropTarget] = bgColor;
		colors[ImGuiCol_NavHighlight] = bgColor;
		colors[ImGuiCol_DockingPreview] = panelActiveColor;
		colors[ImGuiCol_Tab] = bgColor;
		colors[ImGuiCol_TabActive] = panelActiveColor;
		colors[ImGuiCol_TabUnfocused] = bgColor;
		colors[ImGuiCol_TabUnfocusedActive] = panelActiveColor;
		colors[ImGuiCol_TabHovered] = panelHoverColor;

		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 0.0f;
		style.GrabRounding = 0.0f;
		style.PopupRounding = 0.0f;
		style.ScrollbarRounding = 0.0f;
		style.TabRounding = 0.0f;

#ifdef IMGUI_HAS_DOCK 
		style.TabBorderSize = true;
		style.TabRounding = 3;

		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
#endif
	}

	void Editor::HandleInput()
	{
		if (scene_focused && engine->input.IsKeyDown(EKeyCode::I)) gui->ToggleVisibility();

		if (scene_focused && engine->input.IsKeyDown(EKeyCode::G)) gizmo_enabled = !gizmo_enabled;

		if (gizmo_enabled && gui->IsVisible())
		{

			if (engine->input.IsKeyDown(EKeyCode::T)) gizmo_op = ImGuizmo::TRANSLATE;

			if (engine->input.IsKeyDown(EKeyCode::R)) gizmo_op = ImGuizmo::ROTATE;

			if (engine->input.IsKeyDown(EKeyCode::E)) gizmo_op = ImGuizmo::SCALE;
		}

		engine->camera_manager.ShouldUpdate(scene_focused);
	}

	void Editor::MenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Model"))
					ImGuiFileDialog::Instance()->OpenDialog("Choose Model", "Choose File", ".gltf", ".");

				ImGui::EndMenu();
			}

			if (ImGuiFileDialog::Instance()->Display("Choose Model"))
			{

				if (ImGuiFileDialog::Instance()->IsOk())
				{
					std::string model_path = ImGuiFileDialog::Instance()->GetFilePathName();

					ModelParameters params{};
					params.model_path = model_path;
					std::string texture_path = ImGuiFileDialog::Instance()->GetCurrentPath();
					if (!texture_path.empty()) texture_path.append("/");

					params.textures_path = texture_path;
					engine->entity_loader->ImportModel_GLTF(params);
				}

				ImGuiFileDialog::Instance()->Close();
			}

			if (ImGui::BeginMenu("Help"))
			{
				ImGui::Text("Controls\n");
				ImGui::Text(
					"Move Camera with W, A, S, D, Q and E. Use Mouse for Rotating Camera. Use Mouse Scroll for Zoom In/Out.\n"
					"Press I to toggle between Cinema Mode and Editor Mode. (Scene Window has to be active) \n"
					"Press G to toggle Gizmo. (Scene Window has to be active) \n"
					"When Gizmo is enabled, use T, R and E to switch between Translation, Rotation and Scaling Mode.\n"
					"Left Click on entity to select it. Left click again on selected entity to unselect it.\n"
					"Right Click on empty area in Entities window to add entity. Right Click on selected entity to delete it.\n"
					"When placing decals, right click on focused Scene window to pick a point for a decal (it's used only for "
					"decals currently but that could change in the future)"
				);
				ImGui::Spacing();

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}

	void Editor::OceanSettings()
	{
		ImGui::Begin("Ocean");
		{
			static GridParameters ocean_params{};
			static int32 tile_count[2] = { 512, 512 };
			static float32 tile_size[2] = { 40.0f, 40.0f };
			static float32 texture_scale[2] = { 20.0f, 20.0f };

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
				engine->entity_loader->LoadOcean(params);
			}

			if (ImGui::Button("Clear"))
			{
				engine->reg.clear<Ocean>();
			}

			if (ImGui::TreeNodeEx("Ocean Settings", 0))
			{
				ImGui::Checkbox("Tessellation", &renderer_settings.ocean_tesselation);
				ImGui::Checkbox("Wireframe", &renderer_settings.ocean_wireframe);

				ImGui::SliderFloat("Choppiness", &renderer_settings.ocean_choppiness, 0.0f, 10.0f);
				renderer_settings.ocean_color_changed = ImGui::ColorEdit3("Ocean Color", renderer_settings.ocean_color);
				ImGui::TreePop();
				ImGui::Separator();
			}
		}
		ImGui::End();
	}

	void Editor::ParticleSettings()
	{
		ImGui::Begin("Particles");
		{
			static EmitterParameters params{};
			static char NAME_BUFFER[128];
			ImGui::InputText("Name", NAME_BUFFER, sizeof(NAME_BUFFER));
			params.name = std::string(NAME_BUFFER);
			if (ImGui::Button("Select Texture")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
			if (ImGuiFileDialog::Instance()->Display("Choose Texture"))
			{
				if (ImGuiFileDialog::Instance()->IsOk())
				{
					std::wstring texture_path = ConvertToWide(ImGuiFileDialog::Instance()->GetFilePathName());
					params.texture_path = texture_path;
				}
				ImGuiFileDialog::Instance()->Close();
			}
			ImGui::Text(ConvertToNarrow(params.texture_path).c_str());

			ImGui::SliderFloat3("Position", params.position, -500.0f, 500.0f);
			ImGui::SliderFloat3("Velocity", params.velocity, -50.0f, 50.0f);
			ImGui::SliderFloat3("Position Variance", params.position_variance, -50.0f, 50.0f);
			ImGui::SliderFloat("Velocity Variance", &params.velocity_variance, -10.0f, 10.0f);
			ImGui::SliderFloat("Lifespan", &params.lifespan, 0.0f, 50.0f);
			ImGui::SliderFloat("Start Size", &params.start_size, 0.0f, 50.0f);
			ImGui::SliderFloat("End Size", &params.end_size, 0.0f, 10.0f);
			ImGui::SliderFloat("Mass", &params.mass, 0.0f, 10.0f);
			ImGui::SliderFloat("Particles Per Second", &params.particles_per_second, 1.0f, 1000.0f);
			ImGui::Checkbox("Alpha Blend", &params.blend);
			ImGui::Checkbox("Collisions", &params.collisions);
			ImGui::Checkbox("Sort", &params.sort);
			if (params.collisions) ImGui::SliderInt("Collision Thickness", &params.collision_thickness, 0, 40);

			if (ImGui::Button("Load Emitter"))
			{
				entt::entity e = engine->entity_loader->LoadEmitter(params);
				editor_events.particle_emitter_added.Broadcast(entt::to_integral(e));
			}
		}
		ImGui::End();
	}

	void Editor::DecalSettings()
	{
		ImGui::Begin("Decals");
		{
			static DecalParameters params{};
			static char NAME_BUFFER[128];
			ImGui::InputText("Name", NAME_BUFFER, sizeof(NAME_BUFFER));
			params.name = std::string(NAME_BUFFER);
			ImGui::PushID(6);
			if (ImGui::Button("Select Albedo Texture")) ImGuiFileDialog::Instance()->OpenDialog("Choose Albedo Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
			if (ImGuiFileDialog::Instance()->Display("Choose Albedo Texture"))
			{
				if (ImGuiFileDialog::Instance()->IsOk())
				{
					std::string texture_path = ImGuiFileDialog::Instance()->GetFilePathName();
					params.albedo_texture_path = texture_path;
				}
				ImGuiFileDialog::Instance()->Close();
			}
			ImGui::PopID();
			ImGui::Text(params.albedo_texture_path.c_str());

			ImGui::PushID(7);
			if (ImGui::Button("Select Normal Texture")) ImGuiFileDialog::Instance()->OpenDialog("Choose Normal Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
			if (ImGuiFileDialog::Instance()->Display("Choose Normal Texture"))
			{
				if (ImGuiFileDialog::Instance()->IsOk())
				{
					std::string texture_path = ImGuiFileDialog::Instance()->GetFilePathName();
					params.normal_texture_path = texture_path;
				}
				ImGuiFileDialog::Instance()->Close();
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
			if (ImGui::Button("Clear Decals"))
			{
				for(auto e : engine->reg.view<Decal>()) engine->reg.destroy(e);
			}
		}
		ImGui::End();
	}

	void Editor::AddEntities()
	{
		ImGui::Begin("Add Entities");
		{
			//random lights
			{
				ImGui::Text("For Easy Demonstration of Tiled Deferred Rendering");
				static int light_count_to_add = 1;
				ImGui::SliderInt("Light Count", &light_count_to_add, 1, 128);

				if (ImGui::Button("Create Random Lights"))
				{
					static RealRandomGenerator real(0.0f, 1.0f);

					for (int32 i = 0; i < light_count_to_add; ++i)
					{
						LightParameters light_params{};
						light_params.light_data.casts_shadows = false;
						light_params.light_data.color = DirectX::XMVectorSet(real() * 2, real() * 2, real() * 2, 1.0f);
						light_params.light_data.direction = DirectX::XMVectorSet(0.5f, -1.0f, 0.1f, 0.0f);
						light_params.light_data.position = DirectX::XMVectorSet(real() * 200 - 100, real() * 200.0f, real() * 200 - 100, 1.0f);
						light_params.light_data.type = ELightType::Point;
						light_params.mesh_type = ELightMesh::NoMesh;
						light_params.light_data.range = real() * 100.0f + 40.0f;
						light_params.light_data.active = true;
						light_params.light_data.volumetric = false;
						light_params.light_data.volumetric_strength = 1.0f;
						engine->entity_loader->LoadLight(light_params);
					}
				}

			}

		}
		ImGui::End();
	}

	void Editor::SkySettings()
	{
		ImGui::Begin("Sky");
		{
			const char* sky_types[] = { "Skybox", "Uniform Color", "Hosek-Wilkie" };
			static int current_sky_type = 0;
			const char* combo_label = sky_types[current_sky_type];
			if (ImGui::BeginCombo("Sky Type", combo_label, 0))
			{
				for (int n = 0; n < IM_ARRAYSIZE(sky_types); n++)
				{
					const bool is_selected = (current_sky_type == n);
					if (ImGui::Selectable(sky_types[n], is_selected)) current_sky_type = n;
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (current_sky_type == 0) renderer_settings.sky_type = ESkyType::Skybox;
			else if (current_sky_type == 1)
			{
				renderer_settings.sky_type = ESkyType::UniformColor;
				static char const* const sky_colors[] = { "Deep Sky Blue", "Sky Blue", "Light Sky Blue" };
				static int current_sky_color = 0;
				ImGui::ListBox("Tone Map Operator", &current_sky_color, sky_colors, IM_ARRAYSIZE(sky_colors));

				switch (current_sky_color)
				{
				case 0:
				{
					static float32 deep_sky_blue[3] = { 0.0f, 0.75f, 1.0f };
					memcpy(renderer_settings.sky_color, deep_sky_blue, sizeof(deep_sky_blue));
					break;
				}
				case 1:
				{
					static float32 sky_blue[3] = { 0.53f, 0.81f, 0.92f };
					memcpy(renderer_settings.sky_color, sky_blue, sizeof(sky_blue));
					break;
				}
				case 2:
				{
					static float32 light_sky_blue[3] = { 0.53f, 0.81f, 0.98f };
					memcpy(renderer_settings.sky_color, light_sky_blue, sizeof(light_sky_blue));
					break;
				}
				default:
					ADRIA_ASSERT(false);
				}
			}
			else if (current_sky_type == 2)
			{
				renderer_settings.sky_type = ESkyType::HosekWilkie;
				ImGui::SliderFloat("Turbidity", &renderer_settings.turbidity, 2.0f, 30.0f);
				ImGui::SliderFloat("Ground Albedo", &renderer_settings.ground_albedo, 0.0f, 1.0f);
			}

		}
		ImGui::End();
	}

	void Editor::ListEntities()
	{
		auto all_entities = engine->reg.view<Tag>();
		ImGui::Begin("Entities");
		{
			std::vector<entt::entity> deleted_entities{};
			std::function<void(entt::entity, bool)> ShowEntity;
			ShowEntity = [&](entt::entity e, bool first_iteration)
			{
				Relationship* relationship = engine->reg.try_get<Relationship>(e);
				if (first_iteration && relationship && relationship->parent != entt::null) return;
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
					if (relationship)
					{
						for (size_t i = 0; i < relationship->children_count; ++i)
						{
							ShowEntity(relationship->children[i], false);
						}
					}
					ImGui::TreePop();
				}
			};
			for (auto e : all_entities) ShowEntity(e, true);
		}
		ImGui::End();
	}

	void Editor::Properties()
	{
		ImGui::Begin("Properties");
		{
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
					if (light->type == ELightType::Directional)	ImGui::Text("Directional Light");
					else if (light->type == ELightType::Spot)	ImGui::Text("Spot Light");
					else if (light->type == ELightType::Point)	ImGui::Text("Point Light");

					XMFLOAT4 light_color, light_direction, light_position;
					XMStoreFloat4(&light_color, light->color);
					XMStoreFloat4(&light_direction, light->direction);
					XMStoreFloat4(&light_position, light->position);

					float32 color[3] = { light_color.x, light_color.y, light_color.z };
					ImGui::ColorEdit3("Light Color", color);
					light->color = XMVectorSet(color[0], color[1], color[2], 1.0f);

					ImGui::SliderFloat("Light Energy", &light->energy, 0.0f, 50.0f);

					if (engine->reg.all_of<Material>(selected_entity))
					{
						auto& material = engine->reg.get<Material>(selected_entity);
						material.diffuse = XMFLOAT3(color[0], color[1], color[2]);
					}

					if (light->type == ELightType::Directional || light->type == ELightType::Spot)
					{
						float32 direction[3] = { light_direction.x, light_direction.y, light_direction.z };

						ImGui::SliderFloat3("Light direction", direction, -1.0f, 1.0f);

						light->direction = XMVectorSet(direction[0], direction[1], direction[2], 0.0f);

						if (light->type == ELightType::Directional)
						{
							light->position = XMVectorScale(-light->direction, 1e3);
						}
					}

					if (light->type == ELightType::Spot)
					{
						float32 inner_angle = XMConvertToDegrees(acos(light->inner_cosine))
							, outer_angle = XMConvertToDegrees(acos(light->outer_cosine));
						ImGui::SliderFloat("Inner Spot Angle", &inner_angle, 0.0f, 90.0f);
						ImGui::SliderFloat("Outer Spot Angle", &outer_angle, inner_angle, 90.0f);

						light->inner_cosine = cos(XMConvertToRadians(inner_angle));
						light->outer_cosine = cos(XMConvertToRadians(outer_angle));
					}

					if (light->type == ELightType::Point || light->type == ELightType::Spot)
					{
						float32 position[3] = { light_position.x, light_position.y, light_position.z };

						ImGui::SliderFloat3("Light position", position, -300.0f, 500.0f);

						light->position = XMVectorSet(position[0], position[1], position[2], 1.0f);

						ImGui::SliderFloat("Range", &light->range, 50.0f, 1000.0f);
					}

					if (engine->reg.all_of<Transform>(selected_entity))
					{
						auto& tr = engine->reg.get<Transform>(selected_entity);

						tr.current_transform = XMMatrixTranslationFromVector(light->position);
					}

					ImGui::Checkbox("Active", &light->active);

					if (light->type == ELightType::Directional)
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
						light->casts_shadows = (current_shadow_type == 1);
						light->ray_traced_shadows = (current_shadow_type == 2);
					}
					else
					{
						ImGui::Checkbox("Casts Shadows", &light->casts_shadows);
					}

					if (light->casts_shadows)
					{
						if (light->type == ELightType::Directional && light->casts_shadows)
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
						//add softness
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
						ImGui::SliderFloat("Volumetric lighting Strength", &light->volumetric_strength, 0.0f, 5.0f);
					}

					ImGui::Checkbox("Lens Flare", &light->lens_flare);
				}

				auto material = engine->reg.try_get<Material>(selected_entity);
				if (material && ImGui::CollapsingHeader("Material"))
				{
					ID3D12Device5* device = engine->gfx->GetDevice();
					RingOnlineDescriptorAllocator* descriptor_allocator = gui->DescriptorAllocator();

					ImGui::Text("Albedo Texture");
					D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(material->albedo_texture);
					OffsetType descriptor_index = descriptor_allocator->Allocate();
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(0);
					if (ImGui::Button("Remove")) material->albedo_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
					OpenMaterialFileDialog(material, EMaterialTextureType::eAlbedo);
					ImGui::PopID();

					ImGui::Text("Metallic-Roughness Texture");
					tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(material->metallic_roughness_texture);
					descriptor_index = descriptor_allocator->Allocate();
					dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(1);
					if (ImGui::Button("Remove")) material->metallic_roughness_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
					OpenMaterialFileDialog(material, EMaterialTextureType::eMetallicRoughness);
					ImGui::PopID();

					ImGui::Text("Emissive Texture");
					tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(material->emissive_texture);
					descriptor_index = descriptor_allocator->Allocate();
					dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(2);
					if (ImGui::Button("Remove")) material->emissive_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
					OpenMaterialFileDialog(material, EMaterialTextureType::eEmissive);
					ImGui::PopID();

					ImGui::ColorEdit3("Albedo Color", &material->diffuse.x);
					ImGui::SliderFloat("Albedo Factor", &material->albedo_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Metallic Factor", &material->metallic_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Roughness Factor", &material->roughness_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Emissive Factor", &material->emissive_factor, 0.0f, 32.0f);

					//add shader changing
					if (engine->reg.all_of<Forward>(selected_entity))
					{
						if (material->albedo_texture != INVALID_TEXTURE_HANDLE)
							material->pso = EPipelineState::Texture;
						else material->pso = EPipelineState::Solid;
					}
					else
					{
						material->pso = EPipelineState::GBufferPBR;
					}
				}

				auto transform = engine->reg.try_get<Transform>(selected_entity);
				if (transform && ImGui::CollapsingHeader("Transform"))
				{
					XMFLOAT4X4 tr;
					XMStoreFloat4x4(&tr, transform->current_transform);

					float translation[3], rotation[3], scale[3];
					ImGuizmo::DecomposeMatrixToComponents(tr.m[0], translation, rotation, scale);
					bool change = ImGui::InputFloat3("Translation", translation);
					change &= ImGui::InputFloat3("Rotation", rotation);
					change &= ImGui::InputFloat3("Scale", scale);
					ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, tr.m[0]);

					if (Emitter* emitter = engine->reg.try_get<Emitter>(selected_entity))
					{
						emitter->position = XMFLOAT4(translation[0], translation[1], translation[2], 1.0f);
					}

					if (AABB* aabb = engine->reg.try_get<AABB>(selected_entity))
					{
						aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMMatrixInverse(nullptr, transform->current_transform));
						aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMLoadFloat4x4(&tr));
						if(change) aabb_updates.push(aabb);
					}

					if (Relationship* relationship = engine->reg.try_get<Relationship>(selected_entity))
					{
						for (size_t i = 0; i < relationship->children_count; ++i)
						{
							entt::entity child = relationship->children[i];
							if (AABB* aabb = engine->reg.try_get<AABB>(child))
							{
								aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMMatrixInverse(nullptr, transform->current_transform));
								aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMLoadFloat4x4(&tr));
								if (change) aabb_updates.push(aabb);
							}
						}
					}
					transform->current_transform = DirectX::XMLoadFloat4x4(&tr);
				}

				auto emitter = engine->reg.try_get<Emitter>(selected_entity);
				if (emitter && ImGui::CollapsingHeader("Emitter"))
				{
					ID3D12Device5* device = engine->gfx->GetDevice();
					RingOnlineDescriptorAllocator* descriptor_allocator = gui->DescriptorAllocator();

					ImGui::Text("Particle Texture");
					D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(emitter->particle_texture);
					OffsetType descriptor_index = descriptor_allocator->Allocate();
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(3);
					if (ImGui::Button("Remove")) emitter->particle_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
					if (ImGuiFileDialog::Instance()->Display("Choose Texture"))
					{
						if (ImGuiFileDialog::Instance()->IsOk())
						{
							std::wstring texture_path = ConvertToWide(ImGuiFileDialog::Instance()->GetFilePathName());
							emitter->particle_texture = engine->renderer->GetTextureManager().LoadTexture(texture_path);
						}
						ImGuiFileDialog::Instance()->Close();
					}
					ImGui::PopID();

					float32 pos[3] = { emitter->position.x, emitter->position.y, emitter->position.z },
						vel[3] = { emitter->velocity.x, emitter->velocity.y, emitter->velocity.z },
						pos_var[3] = { emitter->position_variance.x, emitter->position_variance.y, emitter->position_variance.z };

					ImGui::SliderFloat3("Position", pos, -500.0f, 500.0f);
					ImGui::SliderFloat3("Velocity", vel, -50.0f, 50.0f);
					ImGui::SliderFloat3("Position Variance", pos_var, -50.0f, 50.0f);
					emitter->position = DirectX::XMFLOAT4(pos[0], pos[1], pos[2], 1.0f);
					emitter->velocity = DirectX::XMFLOAT4(vel[0], vel[1], vel[2], 1.0f);
					emitter->position_variance = DirectX::XMFLOAT4(pos_var[0], pos_var[1], pos_var[2], 1.0f);

					if (transform)
					{
						XMFLOAT4X4 tr;
						XMStoreFloat4x4(&tr, transform->current_transform);
						float32 translation[3], rotation[3], scale[3];
						ImGuizmo::DecomposeMatrixToComponents(tr.m[0], translation, rotation, scale);
						ImGuizmo::RecomposeMatrixFromComponents(pos, rotation, scale, tr.m[0]);
						transform->current_transform = DirectX::XMLoadFloat4x4(&tr);
					}

					ImGui::SliderFloat("Velocity Variance", &emitter->velocity_variance, -10.0f, 10.0f);
					ImGui::SliderFloat("Lifespan", &emitter->particle_lifespan, 0.0f, 50.0f);
					ImGui::SliderFloat("Start Size", &emitter->start_size, 0.0f, 50.0f);
					ImGui::SliderFloat("End Size", &emitter->end_size, 0.0f, 10.0f);
					ImGui::SliderFloat("Mass", &emitter->mass, 0.0f, 10.0f);
					ImGui::SliderFloat("Particles Per Second", &emitter->particles_per_second, 1.0f, 1000.0f);

					ImGui::Checkbox("Alpha Blend", &emitter->alpha_blended);
					ImGui::Checkbox("Collisions", &emitter->collisions_enabled);
					if (emitter->collisions_enabled) ImGui::SliderInt("Collision Thickness", &emitter->collision_thickness, 0, 40);

					ImGui::Checkbox("Sort", &emitter->sort);
					ImGui::Checkbox("Pause", &emitter->pause);
					if (ImGui::Button("Reset")) emitter->reset_emitter = true;
				}

				auto decal = engine->reg.try_get<Decal>(selected_entity);
				if (decal && ImGui::CollapsingHeader("Decal"))
				{
					ID3D12Device5* device = engine->gfx->GetDevice();
					RingOnlineDescriptorAllocator* descriptor_allocator = gui->DescriptorAllocator();

					ImGui::Text("Decal Albedo Texture");
					D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(decal->albedo_decal_texture);
					OffsetType descriptor_index = descriptor_allocator->Allocate();
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(4);
					if (ImGui::Button("Remove")) decal->albedo_decal_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
					if (ImGuiFileDialog::Instance()->Display("Choose Texture"))
					{
						if (ImGuiFileDialog::Instance()->IsOk())
						{
							std::string texture_path = ImGuiFileDialog::Instance()->GetFilePathName();
							decal->albedo_decal_texture = engine->renderer->GetTextureManager().LoadTexture(ConvertToWide(texture_path));
						}
						ImGuiFileDialog::Instance()->Close();
					}
					ImGui::PopID();

					ImGui::Text("Decal Normal Texture");
					tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(decal->normal_decal_texture);
					descriptor_index = descriptor_allocator->Allocate();
					dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr,
						ImVec2(48.0f, 48.0f));

					ImGui::PushID(5);
					if (ImGui::Button("Remove")) decal->normal_decal_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
					if (ImGuiFileDialog::Instance()->Display("Choose Texture"))
					{
						if (ImGuiFileDialog::Instance()->IsOk())
						{
							std::string texture_path = ImGuiFileDialog::Instance()->GetFilePathName();
							decal->normal_decal_texture = engine->renderer->GetTextureManager().LoadTexture(ConvertToWide(texture_path));
						}
						ImGuiFileDialog::Instance()->Close();
					}
					ImGui::PopID();
					ImGui::Checkbox("Modify GBuffer Normals", &decal->modify_gbuffer_normals);
				}

				auto skybox = engine->reg.try_get<Skybox>(selected_entity);
				if (skybox && ImGui::CollapsingHeader("Skybox"))
				{
					ImGui::Checkbox("Active", &skybox->active);
					if (ImGui::Button("Change Skybox Texture")) ImGuiFileDialog::Instance()->OpenDialog("Choose Skybox Texture", "Choose File", ".hdr,.dds", ".");
					if (ImGuiFileDialog::Instance()->Display("Choose Skybox Texture"))
					{
						if (ImGuiFileDialog::Instance()->IsOk())
						{
							std::string texture_path = ImGuiFileDialog::Instance()->GetFilePathName();
							skybox->cubemap_texture = engine->renderer->GetTextureManager().LoadCubemap(ConvertToWide(texture_path));
						}
						ImGuiFileDialog::Instance()->Close();
					}
				}

				auto forward = engine->reg.try_get<Forward>(selected_entity);
				if (forward)
				{
					if (ImGui::CollapsingHeader("Forward")) ImGui::Checkbox("Transparent", &forward->transparent);
				}

				if (AABB* aabb = engine->reg.try_get<AABB>(selected_entity))
				{
					aabb->draw_aabb = true;
				}
			}
		}
		ImGui::End();
	}

	void Editor::Camera()
	{
		auto& camera = engine->camera_manager.GetActiveCamera();

		static bool is_open = true;
		if (is_open)
		{
			if (!ImGui::Begin("Camera", &is_open))
			{
				ImGui::End();
			}
			else
			{
				float32 pos[3] = { camera.Position().m128_f32[0],camera.Position().m128_f32[1], camera.Position().m128_f32[2] };
				ImGui::SliderFloat3("Position", pos, 0.0f, 2000.0f);
				camera.SetPosition(DirectX::XMFLOAT3(pos));
				float32 near_plane = camera.Near(), far_plane = camera.Far();
				float32 _fov = camera.Fov(), _ar = camera.AspectRatio();
				ImGui::SliderFloat("Near Plane", &near_plane, 0.0f, 2.0f);
				ImGui::SliderFloat("Far Plane", &far_plane, 10.0f, 3000.0f);
				ImGui::SliderFloat("FOV", &_fov, 0.01f, 1.5707f);
				camera.SetNearAndFar(near_plane, far_plane);
				camera.SetFov(_fov);
				ImGui::End();
			}
		}
	}

	void Editor::Scene()
	{
		ImGui::Begin("Scene");
		{
			auto device = engine->gfx->GetDevice();
			auto descriptor_allocator = gui->DescriptorAllocator();

			ImVec2 v_min = ImGui::GetWindowContentRegionMin();
			ImVec2 v_max = ImGui::GetWindowContentRegionMax();
			v_min.x += ImGui::GetWindowPos().x;
			v_min.y += ImGui::GetWindowPos().y;
			v_max.x += ImGui::GetWindowPos().x;
			v_max.y += ImGui::GetWindowPos().y;
			ImVec2 size(v_max.x - v_min.x, v_max.y - v_min.y);

			D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetFinalTexture()->GetSubresource_SRV();
			OffsetType descriptor_index = descriptor_allocator->Allocate();
			auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

			auto& camera = engine->camera_manager.GetActiveCamera();

			auto camera_view = camera.View();
			auto camera_proj = camera.Proj();

			DirectX::XMFLOAT4X4 view, projection;

			DirectX::XMStoreFloat4x4(&view, camera_view);
			DirectX::XMStoreFloat4x4(&projection, camera_proj);

			auto& entity_transform = engine->reg.get<Transform>(selected_entity);

			DirectX::XMFLOAT4X4 tr;
			DirectX::XMStoreFloat4x4(&tr, entity_transform.current_transform);

			bool change = ImGuizmo::Manipulate(view.m[0], projection.m[0], gizmo_op, ImGuizmo::LOCAL,
				tr.m[0]);

			if (ImGuizmo::IsUsing())
			{
				AABB* aabb = engine->reg.try_get<AABB>(selected_entity);
				if (aabb)
				{
					aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMMatrixInverse(nullptr, entity_transform.current_transform));
					aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMLoadFloat4x4(&tr));
					if(change) aabb_updates.push(aabb);
				}

				if (Relationship* relationship = engine->reg.try_get<Relationship>(selected_entity))
				{
					for (size_t i = 0; i < relationship->children_count; ++i)
					{
						entt::entity child = relationship->children[i];
						if (AABB* aabb = engine->reg.try_get<AABB>(child))
						{
							aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMMatrixInverse(nullptr, entity_transform.current_transform));
							aabb->bounding_box.Transform(aabb->bounding_box, DirectX::XMLoadFloat4x4(&tr));
							if (change) aabb_updates.push(aabb);
						}
					}
				}
				entity_transform.current_transform = DirectX::XMLoadFloat4x4(&tr);
			}
		}

		ImGui::End();
	}

	void Editor::Log()
	{
		ImGui::Begin("Log");
		{
			editor_log->Draw("Log");
		}
		ImGui::End();
	}

	void Editor::RendererSettings()
	{
		ImGui::Begin("Renderer Settings");
		{
			if (ImGui::TreeNode("Deferred Settings"))
			{
				static const char* deferred_types[] = { "Regular", "Tiled", "Clustered" };
				static int current_deferred_type = 0;
				const char* combo_label = deferred_types[current_deferred_type];
				if (ImGui::BeginCombo("Deferred Type", combo_label, 0))
				{
					for (int n = 0; n < IM_ARRAYSIZE(deferred_types); n++)
					{
						const bool is_selected = (current_deferred_type == n);
						if (ImGui::Selectable(deferred_types[n], is_selected)) current_deferred_type = n;
						if (is_selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				renderer_settings.use_tiled_deferred = (current_deferred_type == 1);
				renderer_settings.use_clustered_deferred = (current_deferred_type == 2);

				if (renderer_settings.use_tiled_deferred && ImGui::TreeNodeEx("Tiled Deferred", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::Checkbox("Visualize Tiles", &renderer_settings.visualize_tiled);
					if (renderer_settings.visualize_tiled) ImGui::SliderInt("Visualize Scale", &renderer_settings.visualize_max_lights, 1, 32);

					ImGui::TreePop();
					ImGui::Separator();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Postprocessing"))
			{
				//ambient oclussion
				{
					static const char* ao_types[] = { "None", "SSAO", "HBAO", "RTAO" };
					static int current_ao_type = 0;
					const char* combo_label = ao_types[current_ao_type];
					if (ImGui::BeginCombo("Ambient Occlusion", combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(ao_types); n++)
						{
							const bool is_selected = (current_ao_type == n);
							if (ImGui::Selectable(ao_types[n], is_selected)) current_ao_type = n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					renderer_settings.postprocessor.ambient_occlusion = static_cast<EAmbientOcclusion>(current_ao_type);
					if (renderer_settings.postprocessor.ambient_occlusion == EAmbientOcclusion::SSAO && ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
					{
						ImGui::SliderFloat("Power", &renderer_settings.postprocessor.ssao_power, 1.0f, 16.0f);
						ImGui::SliderFloat("Radius", &renderer_settings.postprocessor.ssao_radius, 0.5f, 4.0f);

						ImGui::TreePop();
						ImGui::Separator();
					}
					else if (renderer_settings.postprocessor.ambient_occlusion == EAmbientOcclusion::HBAO && ImGui::TreeNodeEx("HBAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
					{
						ImGui::SliderFloat("Power", &renderer_settings.postprocessor.hbao_power, 1.0f, 16.0f);
						ImGui::SliderFloat("Radius", &renderer_settings.postprocessor.hbao_radius, 0.25f, 8.0f);

						ImGui::TreePop();
						ImGui::Separator();
					}
					else if (renderer_settings.postprocessor.ambient_occlusion == EAmbientOcclusion::RTAO && ImGui::TreeNodeEx("RTAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
					{
						ImGui::SliderFloat("Radius", &renderer_settings.postprocessor.rtao_radius, 0.25f, 8.0f);

						ImGui::TreePop();
						ImGui::Separator();
					}
				}
				//reflections
				{
					static const char* reflection_types[] = { "None", "SSR", "RTR" };
					static int current_reflection_type = 0;
					const char* combo_label = reflection_types[current_reflection_type];
					if (ImGui::BeginCombo("Reflections", combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(reflection_types); n++)
						{
							const bool is_selected = (current_reflection_type == n);
							if (ImGui::Selectable(reflection_types[n], is_selected)) current_reflection_type = n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					renderer_settings.postprocessor.reflections = static_cast<EReflections>(current_reflection_type);
					if (renderer_settings.postprocessor.reflections == EReflections::SSR && ImGui::TreeNodeEx("Screen-Space Reflections", 0))
					{
						ImGui::SliderFloat("Ray Step", &renderer_settings.postprocessor.ssr_ray_step, 1.0f, 3.0f);
						ImGui::SliderFloat("Ray Hit Threshold", &renderer_settings.postprocessor.ssr_ray_hit_threshold, 0.25f, 5.0f);

						ImGui::TreePop();
						ImGui::Separator();
					}
				}
				ImGui::Checkbox("Volumetric Clouds", &renderer_settings.postprocessor.clouds);
				ImGui::Checkbox("DoF", &renderer_settings.postprocessor.dof);
				ImGui::Checkbox("Bloom", &renderer_settings.postprocessor.bloom);
				ImGui::Checkbox("Motion Blur", &renderer_settings.postprocessor.motion_blur);
				ImGui::Checkbox("Fog", &renderer_settings.postprocessor.fog);

				if (ImGui::TreeNode("Anti-Aliasing"))
				{
					static bool fxaa = false, taa = false;
					ImGui::Checkbox("FXAA", &fxaa);
					ImGui::Checkbox("TAA", &taa);
					if (fxaa) renderer_settings.postprocessor.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.postprocessor.anti_aliasing | AntiAliasing_FXAA);
					else renderer_settings.postprocessor.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.postprocessor.anti_aliasing & (~AntiAliasing_FXAA));

					if (taa) renderer_settings.postprocessor.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.postprocessor.anti_aliasing | AntiAliasing_TAA);
					else renderer_settings.postprocessor.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.postprocessor.anti_aliasing & (~AntiAliasing_TAA));

					ImGui::TreePop();
				}
				if (renderer_settings.postprocessor.clouds && ImGui::TreeNodeEx("Volumetric Clouds", 0))
				{
					ImGui::SliderFloat("Sun light absorption", &renderer_settings.postprocessor.light_absorption, 0.0f, 0.015f);
					ImGui::SliderFloat("Clouds bottom height", &renderer_settings.postprocessor.clouds_bottom_height, 1000.0f, 10000.0f);
					ImGui::SliderFloat("Clouds top height", &renderer_settings.postprocessor.clouds_top_height, 10000.0f, 50000.0f);
					ImGui::SliderFloat("Density", &renderer_settings.postprocessor.density_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Crispiness", &renderer_settings.postprocessor.crispiness, 0.0f, 100.0f);
					ImGui::SliderFloat("Curliness", &renderer_settings.postprocessor.curliness, 0.0f, 5.0f);
					ImGui::SliderFloat("Coverage", &renderer_settings.postprocessor.coverage, 0.0f, 1.0f);
					ImGui::SliderFloat("Wind speed factor", &renderer_settings.postprocessor.wind_speed, 0.0f, 100.0f);
					ImGui::SliderFloat("Cloud Type", &renderer_settings.postprocessor.cloud_type, 0.0f, 1.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}

				if (renderer_settings.postprocessor.dof && ImGui::TreeNodeEx("Depth Of Field", 0))
				{

					ImGui::SliderFloat("DoF Near Blur", &renderer_settings.postprocessor.dof_near_blur, 0.0f, 200.0f);
					ImGui::SliderFloat("DoF Near", &renderer_settings.postprocessor.dof_near, renderer_settings.postprocessor.dof_near_blur, 500.0f);
					ImGui::SliderFloat("DoF Far", &renderer_settings.postprocessor.dof_far, renderer_settings.postprocessor.dof_near, 1000.0f);
					ImGui::SliderFloat("DoF Far Blur", &renderer_settings.postprocessor.dof_far_blur, renderer_settings.postprocessor.dof_far, 1500.0f);
					ImGui::Checkbox("Bokeh", &renderer_settings.postprocessor.bokeh);

					if (renderer_settings.postprocessor.bokeh)
					{
						static char const* const bokeh_types[] = { "HEXAGON", "OCTAGON", "CIRCLE", "CROSS" };
						static int bokeh_type_i = static_cast<int>(renderer_settings.postprocessor.bokeh_type);
						ImGui::ListBox("Bokeh Type", &bokeh_type_i, bokeh_types, IM_ARRAYSIZE(bokeh_types));
						renderer_settings.postprocessor.bokeh_type = static_cast<EBokehType>(bokeh_type_i);

						ImGui::SliderFloat("Bokeh Blur Threshold", &renderer_settings.postprocessor.bokeh_blur_threshold, 0.0f, 1.0f);
						ImGui::SliderFloat("Bokeh Lum Threshold", &renderer_settings.postprocessor.bokeh_lum_threshold, 0.0f, 10.0f);
						ImGui::SliderFloat("Bokeh Color Scale", &renderer_settings.postprocessor.bokeh_color_scale, 0.1f, 10.0f);
						ImGui::SliderFloat("Bokeh Max Size", &renderer_settings.postprocessor.bokeh_radius_scale, 0.0f, 100.0f);
						ImGui::SliderFloat("Bokeh Fallout", &renderer_settings.postprocessor.bokeh_fallout, 0.0f, 2.0f);
					}

					ImGui::TreePop();
					ImGui::Separator();

				}
				if (renderer_settings.postprocessor.bloom && ImGui::TreeNodeEx("Bloom", 0))
				{
					ImGui::SliderFloat("Threshold", &renderer_settings.postprocessor.bloom_threshold, 0.1f, 2.0f);
					ImGui::SliderFloat("Bloom Scale", &renderer_settings.postprocessor.bloom_scale, 0.1f, 5.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
				if ((renderer_settings.postprocessor.motion_blur || (renderer_settings.postprocessor.anti_aliasing & AntiAliasing_TAA)) && ImGui::TreeNodeEx("Velocity Buffer", 0))
				{
					ImGui::SliderFloat("Velocity Buffer Scale", &renderer_settings.postprocessor.velocity_buffer_scale, 32.0f, 128.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
				if (renderer_settings.postprocessor.fog && ImGui::TreeNodeEx("Fog", 0))
				{
					static const char* fog_types[] = { "Exponential", "Exponential Height" };
					static int current_fog_type = 0; // Here we store our selection data as an index.
					const char* combo_label = fog_types[current_fog_type];  // Label to preview before opening the combo (technically it could be anything)
					if (ImGui::BeginCombo("Fog Type", combo_label, 0))
					{
						for (int n = 0; n < IM_ARRAYSIZE(fog_types); n++)
						{
							const bool is_selected = (current_fog_type == n);
							if (ImGui::Selectable(fog_types[n], is_selected)) current_fog_type = n;
							if (is_selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					renderer_settings.postprocessor.fog_type = static_cast<EFogType>(current_fog_type);

					ImGui::SliderFloat("Fog Falloff", &renderer_settings.postprocessor.fog_falloff, 0.0001f, 0.01f);
					ImGui::SliderFloat("Fog Density", &renderer_settings.postprocessor.fog_density, 0.0001f, 0.01f);
					ImGui::SliderFloat("Fog Start", &renderer_settings.postprocessor.fog_start, 0.1f, 10000.0f);
					ImGui::ColorEdit3("Fog Color", renderer_settings.postprocessor.fog_color);

					ImGui::TreePop();
					ImGui::Separator();
				}
				if (ImGui::TreeNodeEx("Tone Mapping", 0))
				{
					ImGui::SliderFloat("Exposure", &renderer_settings.postprocessor.tonemap_exposure, 0.01f, 10.0f);
					static char const* const operators[] = { "REINHARD", "HABLE", "LINEAR" };
					static int tone_map_operator = static_cast<int>(renderer_settings.postprocessor.tone_map_op);
					ImGui::ListBox("Tone Map Operator", &tone_map_operator, operators, IM_ARRAYSIZE(operators));
					renderer_settings.postprocessor.tone_map_op = static_cast<EToneMap>(tone_map_operator);
					ImGui::TreePop();
					ImGui::Separator();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Misc"))
			{
				renderer_settings.recreate_initial_spectrum = ImGui::SliderFloat2("Wind Direction", renderer_settings.wind_direction, 0.0f, 50.0f);
				ImGui::SliderFloat("Wind speed factor", &renderer_settings.postprocessor.wind_speed, 0.0f, 100.0f);
				ImGui::ColorEdit3("Ambient Color", renderer_settings.ambient_color);
				ImGui::SliderFloat("Blur Sigma", &renderer_settings.blur_sigma, 0.1f, 10.0f);
				ImGui::SliderFloat("Shadow Softness", &renderer_settings.shadow_softness, 0.01f, 5.0f);
				ImGui::Checkbox("Transparent Shadows", &renderer_settings.shadow_transparent);
				ImGui::Checkbox("IBL", &renderer_settings.ibl);
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	void Editor::Profiling()
	{
		if (ImGui::Begin("Profiling"))
		{
			ImGuiIO io = ImGui::GetIO();
			static bool enable_profiling = false;
			ImGui::Checkbox("Enable Profiling", &enable_profiling);
			if (enable_profiling)
			{
				static ProfilerState state;
				if (ImGui::CollapsingHeader("Profiler Settings", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Profile GBuffer Pass", &profiler_settings.profile_gbuffer_pass);
					ImGui::Checkbox("Profile Decal Pass", &profiler_settings.profile_decal_pass);
					ImGui::Checkbox("Profile Deferred Pass", &profiler_settings.profile_deferred_pass);
					ImGui::Checkbox("Profile Forward Pass", &profiler_settings.profile_forward_pass);
					ImGui::Checkbox("Profile Particles Pass", &profiler_settings.profile_particles_pass);
					ImGui::Checkbox("Profile Postprocessing", &profiler_settings.profile_postprocessing);
				}
				engine->renderer->SetProfilerSettings(profiler_settings);

				static constexpr uint64 NUM_FRAMES = 128;
				static float32 FRAME_TIME_ARRAY[NUM_FRAMES] = { 0 };
				static float32 RECENT_HIGHEST_FRAME_TIME = 0.0f;
				static constexpr int32 FRAME_TIME_GRAPH_MAX_FPS[] = { 800, 240, 120, 90, 65, 45, 30, 15, 10, 5, 4, 3, 2, 1 };
				static float32 FRAME_TIME_GRAPH_MAX_VALUES[ARRAYSIZE(FRAME_TIME_GRAPH_MAX_FPS)] = { 0 };
				for (uint64 i = 0; i < ARRAYSIZE(FRAME_TIME_GRAPH_MAX_FPS); ++i) { FRAME_TIME_GRAPH_MAX_VALUES[i] = 1000.f / FRAME_TIME_GRAPH_MAX_FPS[i]; }

				std::vector<Timestamp> time_stamps = engine->renderer->GetProfilerResults();
				FRAME_TIME_ARRAY[NUM_FRAMES - 1] = 1000.0f / io.Framerate;
				for (uint32 i = 0; i < NUM_FRAMES - 1; i++) FRAME_TIME_ARRAY[i] = FRAME_TIME_ARRAY[i + 1];
				RECENT_HIGHEST_FRAME_TIME = std::max(RECENT_HIGHEST_FRAME_TIME, FRAME_TIME_ARRAY[NUM_FRAMES - 1]);
				float32 frameTime_ms = FRAME_TIME_ARRAY[NUM_FRAMES - 1];
				const int32 fps = static_cast<int32>(1000.0f / frameTime_ms);

				ImGui::Text("FPS        : %d (%.2f ms)", fps, frameTime_ms);
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
						for (uint32_t i = 0; i < state.displayed_timestamps.size(); i++)
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

					for (uint64 i = 0; i < time_stamps.size(); i++)
					{
						float32 value = time_stamps[i].time_in_ms;
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
					}
					state.accumulating_frame_count++;
				}
			}
			else
			{
				engine->renderer->SetProfilerSettings(NO_PROFILING);
			}
			static bool display_vram_usage = false;
			ImGui::Checkbox("Display VRAM Usage", &display_vram_usage);
			if (display_vram_usage)
			{
				GPUMemoryUsage vram = engine->gfx->GetMemoryUsage();
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
		if (ImGui::Begin("Shader Hot Reload"))
		{
			if (ImGui::Button("Compile Changed Shaders"))
			{
				reload_shaders = true;
			}
		}
		ImGui::End();
	}

	void Editor::RayTracingDebug()
	{
#ifdef _DEBUG
		auto device = engine->gfx->GetDevice();
		auto descriptor_allocator = gui->DescriptorAllocator();
		ImVec2 v_min = ImGui::GetWindowContentRegionMin();
		ImVec2 v_max = ImGui::GetWindowContentRegionMax();
		v_min.x += ImGui::GetWindowPos().x;
		v_min.y += ImGui::GetWindowPos().y;
		v_max.x += ImGui::GetWindowPos().x;
		v_max.y += ImGui::GetWindowPos().y;
		ImVec2 size(v_max.x - v_min.x, v_max.y - v_min.y);

		ImGui::Begin("Ray Tracing Debug");
		{
			static const char* rt_types[] = { "Shadows", "Ambient Occlusion", "Reflections" };
			static int current_rt_type = 0;
			const char* combo_label = rt_types[current_rt_type];
			if (ImGui::BeginCombo("RT Texture Type", combo_label, 0))
			{
				for (int n = 0; n < IM_ARRAYSIZE(rt_types); n++)
				{
					const bool is_selected = (current_rt_type == n);
					if (ImGui::Selectable(rt_types[n], is_selected)) current_rt_type = n;
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (current_rt_type == 0)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetRTSDebugTexture()->GetSubresource_SRV();
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);
				ImGui::Text("Ray Tracing Shadows Image");
			}
			else if (current_rt_type == 1)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetRTAODebugTexture()->GetSubresource_SRV();
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);
				ImGui::Text("Ray Tracing AO Image");
			}
			else if (current_rt_type == 2)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetRTSDebugTexture()->GetSubresource_SRV();
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);
				ImGui::Text("Ray Tracing Reflections Image");
			}

		}
		ImGui::End();
#endif
	}

	void Editor::OpenMaterialFileDialog(Material* material, EMaterialTextureType type)
	{
		if (ImGuiFileDialog::Instance()->Display("Choose Texture"))
		{
			if (ImGuiFileDialog::Instance()->IsOk())
			{
				std::wstring texture_path = ConvertToWide(ImGuiFileDialog::Instance()->GetFilePathName());

				switch (type)
				{
				case EMaterialTextureType::eAlbedo:
					material->albedo_texture = engine->renderer->GetTextureManager().LoadTexture(texture_path);
					break;
				case EMaterialTextureType::eMetallicRoughness:
					material->metallic_roughness_texture = engine->renderer->GetTextureManager().LoadTexture(texture_path);
					break;
				case EMaterialTextureType::eEmissive:
					material->emissive_texture = engine->renderer->GetTextureManager().LoadTexture(texture_path);
					break;
				}
			}

			ImGuiFileDialog::Instance()->Close();
		}
	}
}
