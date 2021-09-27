#include "Editor.h"

#include "../Rendering/Renderer.h"
#include "../Core/Events.h"
#include "../Graphics/GraphicsCoreDX12.h"
#include "../Rendering/EntityLoader.h"
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
    using namespace tecs;

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////// PUBLIC //////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    struct EditorLogger
    {
		ImGuiTextBuffer     Buf;
		ImGuiTextFilter     Filter;
		ImVector<int>       LineOffsets;
		bool                AutoScroll;

		EditorLogger()
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

	enum class MaterialTextureType
	{
		eAlbedo,
		eMetallicRoughness,
		eEmissive
	};


    Editor::Editor(editor_init_t const& init) : engine(), editor_log(new EditorLogger{})
    {
        Log::Initialize(init.log_file);
        Log::AddLogCallback([this](std::string const& s) { editor_log->AddLog(s.c_str()); });

        engine = std::make_unique<Engine>(init.engine_init);
        gui = std::make_unique<GUI>(engine->gfx.get());

        SetStyle();
    }

    Editor::~Editor() = default;

	void Editor::HandleWindowMessage(window_message_t const& msg_data)
    {
        engine->HandleWindowMessage(msg_data);
        gui->HandleWindowMessage(msg_data);
    }

    void Editor::Run()
    {
        HandleInput();

        if (gui->IsVisible())
        {
            engine->Run(renderer_settings, true);
            auto gui_cmd_list = engine->gfx->GetNewGraphicsCommandList();
            engine->gfx->SetBackbuffer(gui_cmd_list);
            {
                PIXScopedEvent(gui_cmd_list, PIX_COLOR_DEFAULT, "GUI Pass");
                gui->Begin();
                MenuBar();
                auto dockspace_id = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
                ListEntities();
                AddEntities();
                Camera();
                Scene();
                RendererSettings();
                Properties();
                Log();
                Profiling();
                gui->End(gui_cmd_list);
            }
            engine->Present();
        }
        else
        {
            engine->Run(renderer_settings, false);
            engine->Present();
        }
    }


    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////// PRIVATE /////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    void Editor::SetStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.GrabRounding = 0.f;
        style.WindowRounding = 0.f;
        style.ScrollbarRounding = 3.f;
        style.FrameRounding = 3.f;
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

        style.Colors[ImGuiCol_Text] = ImVec4(0.73f, 0.73f, 0.73f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.26f, 0.26f, 0.26f, 0.95f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.32f, 0.52f, 0.65f, 1.00f);

    }

    void Editor::HandleInput()
    {
        if (mouse_in_scene && engine->input.IsKeyDown(KeyCode::I)) gui->ToggleVisibility();

        if (mouse_in_scene && engine->input.IsKeyDown(KeyCode::G)) gizmo_enabled = !gizmo_enabled;

        if (gizmo_enabled && gui->IsVisible())
        {

            if (engine->input.IsKeyDown(KeyCode::T)) gizmo_op = ImGuizmo::TRANSLATE;

            if (engine->input.IsKeyDown(KeyCode::R)) gizmo_op = ImGuizmo::ROTATE;

            if (engine->input.IsKeyDown(KeyCode::E)) gizmo_op = ImGuizmo::SCALE; //e because s is for camera movement and its close to wasd and tr

        }

        engine->camera_manager.ShouldUpdate(mouse_in_scene);
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

                    model_parameters_t params{};
                    params.model_path = model_path;
                    std::string texture_path = ImGuiFileDialog::Instance()->GetCurrentPath();
                    if (!texture_path.empty()) texture_path.append("/");

                    params.textures_path = texture_path;
                    engine->entity_loader->LoadGLTFModel(params);
                }

                ImGuiFileDialog::Instance()->Close();
            }

            if (ImGui::BeginMenu("Help"))
            {
                ImGui::Text("Controls\n");
                ImGui::Text(
                    "Move Camera with W, A, S, D and Mouse. Use Mouse Scroll for Zoom In/Out.\n"
                    "Press I to toggle between Cinema Mode and Editor Mode. (Scene Window has to be active) \n"
                    "Press G to toggle Gizmo. (Scene Window has to be active) \n"
                    "When Gizmo is enabled, use T, R and E to switch between Translation, Rotation and Scaling Mode.\n"
                    "Left Click on entity to select it. Left click again on selected entity to unselect it.\n"
                    "Right Click on empty area in Entities window to add entity. Right Click on selected entity to delete it.\n"

                );
                ImGui::Spacing();

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
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

                    for (i32 i = 0; i < light_count_to_add; ++i)
                    {
                        light_parameters_t light_params{};
                        light_params.light_data.casts_shadows = false;
                        light_params.light_data.color = DirectX::XMVectorSet(real() * 2, real() * 2, real() * 2, 1.0f);
                        light_params.light_data.direction = DirectX::XMVectorSet(0.5f, -1.0f, 0.1f, 0.0f);
                        light_params.light_data.position = DirectX::XMVectorSet(real() * 500 - 250, real() * 500.0f, real() * 500 - 250, 1.0f);
                        light_params.light_data.type = LightType::ePoint;
                        light_params.mesh_type = LightMesh::eNoMesh;
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

    void Editor::ListEntities()
    {
        auto all_entities = engine->reg.view<Tag>();

        ImGui::Begin("Entities");
        {
            if (ImGui::BeginPopupContextWindow(0, 1, false))
            {
                if (ImGui::MenuItem("Create"))
                {
                    entity empty = engine->reg.create();
                    engine->reg.emplace<Tag>(empty);
                }

                ImGui::EndPopup();
            }

            std::vector<entity> deleted_entities{};
            for (auto e : all_entities)
            {
                auto& tag = all_entities.get(e);

                ImGuiTreeNodeFlags flags = ((selected_entity == e) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
                flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
                bool opened = ImGui::TreeNodeEx(tag.name.c_str(), flags);


                if (ImGui::IsItemClicked())
                {
                    if (e == selected_entity)
                        selected_entity = null_entity;
                    else
                        selected_entity = e;
                }


                bool entity_deleted = false;
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Delete")) entity_deleted = true;

                    ImGui::EndPopup();
                }

                if (opened) ImGui::TreePop();

                if (entity_deleted)
                {
                    deleted_entities.push_back(e);
                    if (selected_entity == e) selected_entity = null_entity;
                }


            }

            for (auto e : deleted_entities)
                engine->reg.destroy(e);


        }
        ImGui::End();
    }

    void Editor::Properties()
    {
        ImGui::Begin("Properties");
        {
            if (selected_entity != null_entity)
            {
                auto tag = engine->reg.get_if<Tag>(selected_entity);
                if (tag)
                {
                    char buffer[256];
                    memset(buffer, 0, sizeof(buffer));
                    std::strncpy(buffer, tag->name.c_str(), sizeof(buffer));
                    if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
                        tag->name = std::string(buffer);
                }

                auto light = engine->reg.get_if<Light>(selected_entity);
                if (light)
                {
                    if (ImGui::CollapsingHeader("Light"))
                    {

                        if (light->type == LightType::eDirectional)			ImGui::Text("Directional Light");
                        else if (light->type == LightType::eSpot)			ImGui::Text("Spot Light");
                        else if (light->type == LightType::ePoint)			ImGui::Text("Point Light");

                        XMFLOAT4 light_color, light_direction, light_position;
                        XMStoreFloat4(&light_color, light->color);
                        XMStoreFloat4(&light_direction, light->direction);
                        XMStoreFloat4(&light_position, light->position);

                        f32 color[3] = { light_color.x, light_color.y, light_color.z };

                        ImGui::ColorEdit3("Light Color", color);

                        light->color = XMVectorSet(color[0], color[1], color[2], 1.0f);

                        ImGui::SliderFloat("Light Energy", &light->energy, 0.0f, 50.0f);

                        if (engine->reg.has<Material>(selected_entity))
                        {
                            auto& material = engine->reg.get<Material>(selected_entity);
                            material.diffuse = XMFLOAT3(color[0], color[1], color[2]);
                        }

                        if (light->type == LightType::eDirectional || light->type == LightType::eSpot)
                        {
                            f32 direction[3] = { light_direction.x, light_direction.y, light_direction.z };

                            ImGui::SliderFloat3("Light direction", direction, -1.0f, 1.0f);

                            light->direction = XMVectorSet(direction[0], direction[1], direction[2], 0.0f);

                            if (light->type == LightType::eDirectional)
                            {
                                light->position = XMVectorScale(-light->direction, 1e3);
                            }
                        }

                        if (light->type == LightType::eSpot)
                        {
                            f32 inner_angle = XMConvertToDegrees(acos(light->inner_cosine))
                                , outer_angle = XMConvertToDegrees(acos(light->outer_cosine));
                            ImGui::SliderFloat("Inner Spot Angle", &inner_angle, 0.0f, 90.0f);
                            ImGui::SliderFloat("Outer Spot Angle", &outer_angle, inner_angle, 90.0f);

                            light->inner_cosine = cos(XMConvertToRadians(inner_angle));
                            light->outer_cosine = cos(XMConvertToRadians(outer_angle));
                        }

                        if (light->type == LightType::ePoint || light->type == LightType::eSpot)
                        {
                            f32 position[3] = { light_position.x, light_position.y, light_position.z };

                            ImGui::SliderFloat3("Light position", position, -300.0f, 500.0f);

                            light->position = XMVectorSet(position[0], position[1], position[2], 1.0f);

                            ImGui::SliderFloat("Range", &light->range, 50.0f, 1000.0f);
                        }

                        if (engine->reg.has<Transform>(selected_entity))
                        {
                            auto& tr = engine->reg.get<Transform>(selected_entity);

                            tr.current_transform = XMMatrixTranslationFromVector(light->position);
                        }

                        ImGui::Checkbox("Active", &light->active);
                        ImGui::Checkbox("God Rays", &light->god_rays);

                        if (light->god_rays)
                        {
                            ImGui::SliderFloat("God Rays decay", &light->godrays_decay, 0.0f, 1.0f);
                            ImGui::SliderFloat("God Rays weight", &light->godrays_weight, 0.0f, 1.0f);
                            ImGui::SliderFloat("God Rays density", &light->godrays_density, 0.1f, 2.0f);
                            ImGui::SliderFloat("God Rays exposure", &light->godrays_exposure, 0.1f, 10.0f);
                        }


                        ImGui::Checkbox("Casts Shadows", &light->casts_shadows);
                        ImGui::Checkbox("Screen Space Shadows", &light->screenspace_shadows);
                        ImGui::Checkbox("Volumetric Lighting", &light->volumetric);

                        if (light->volumetric)
                        {
                            ImGui::SliderFloat("Volumetric lighting Strength", &light->volumetric_strength, 0.0f, 5.0f);
                        }

                        ImGui::Checkbox("Lens Flare", &light->lens_flare);

                        if (light->type == LightType::eDirectional && light->casts_shadows)
                        {
                            bool use_cascades = static_cast<bool>(light->use_cascades);
                            ImGui::Checkbox("Use Cascades", &use_cascades);
                            light->use_cascades = use_cascades;
                        }

                    }
                }

                auto material = engine->reg.get_if<Material>(selected_entity);
                if (material)
                {
                    if (ImGui::CollapsingHeader("Material"))
                    {
                        auto device = engine->gfx->GetDevice();
                        auto descriptor_allocator = gui->DescriptorAllocator();

                        ImGui::Text("Albedo Texture");
                        D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(material->albedo_texture);
                        OffsetType descriptor_index = descriptor_allocator->Allocate();
                        D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
                        device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                        ImGui::Image((ImTextureID)descriptor_allocator->GetGpuHandle(descriptor_index).ptr,
                            ImVec2(48.0f, 48.0f));

                        ImGui::PushID(0);
                        if (ImGui::Button("Remove")) material->albedo_texture = INVALID_TEXTURE_HANDLE;
                        if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
                        OpenMaterialFileDialog(material, MaterialTextureType::eAlbedo);
                        ImGui::PopID();

                        ImGui::Text("Metallic-Roughness Texture");
                        tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(material->metallic_roughness_texture);
                        descriptor_index = descriptor_allocator->Allocate();
                        dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
                        device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                        ImGui::Image((ImTextureID)descriptor_allocator->GetGpuHandle(descriptor_index).ptr,
                            ImVec2(48.0f, 48.0f));

                        ImGui::PushID(1);
                        if (ImGui::Button("Remove")) material->metallic_roughness_texture = INVALID_TEXTURE_HANDLE;
                        if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
                        OpenMaterialFileDialog(material, MaterialTextureType::eMetallicRoughness);
                        ImGui::PopID();

                        ImGui::Text("Emissive Texture");
                        tex_handle = engine->renderer->GetTextureManager().CpuDescriptorHandle(material->emissive_texture);
                        descriptor_index = descriptor_allocator->Allocate();
                        dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
                        device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                        ImGui::Image((ImTextureID)descriptor_allocator->GetGpuHandle(descriptor_index).ptr,
                            ImVec2(48.0f, 48.0f));

                        ImGui::PushID(2);
                        if (ImGui::Button("Remove")) material->emissive_texture = INVALID_TEXTURE_HANDLE;
                        if (ImGui::Button("Select")) ImGuiFileDialog::Instance()->OpenDialog("Choose Texture", "Choose File", ".jpg,.jpeg,.tga,.dds,.png", ".");
                        OpenMaterialFileDialog(material, MaterialTextureType::eEmissive);
                        ImGui::PopID();

                        ImGui::ColorEdit3("Albedo Color", &material->diffuse.x);
                        ImGui::SliderFloat("Albedo Factor", &material->albedo_factor, 0.0f, 1.0f);
                        ImGui::SliderFloat("Metallic Factor", &material->metallic_factor, 0.0f, 1.0f);
                        ImGui::SliderFloat("Roughness Factor", &material->roughness_factor, 0.0f, 1.0f);
                        ImGui::SliderFloat("Emissive Factor", &material->emissive_factor, 0.0f, 32.0f);

                        //add shader changing
                        if (engine->reg.has<Forward>(selected_entity))
                        {
                            if (material->albedo_texture != INVALID_TEXTURE_HANDLE)
                                material->pso = PSO::eTexture;
                            else material->pso = PSO::eSolid;
                        }
                        else
                        {
                            material->pso = PSO::eGbufferPBR;
                        }
                    }

                }

                auto transform = engine->reg.get_if<Transform>(selected_entity);
                if (transform)
                {
                    if (ImGui::CollapsingHeader("Transform"))
                    {
                        XMFLOAT4X4 tr;
                        XMStoreFloat4x4(&tr, transform->current_transform);

                        float translation[3], rotation[3], scale[3];
                        ImGuizmo::DecomposeMatrixToComponents(tr.m[0], translation, rotation, scale);
                        ImGui::InputFloat3("Translation", translation);
                        ImGui::InputFloat3("Rotation", rotation);
                        ImGui::InputFloat3("Scale", scale);
                        ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, tr.m[0]);

                        Visibility* vis = engine->reg.get_if<Visibility>(selected_entity);

                        if (vis)
                        {
                            vis->aabb.Transform(vis->aabb, DirectX::XMMatrixInverse(nullptr, transform->current_transform));
                            transform->current_transform = DirectX::XMLoadFloat4x4(&tr);
                            vis->aabb.Transform(vis->aabb, transform->current_transform);
                        }
                        else transform->current_transform = DirectX::XMLoadFloat4x4(&tr);
                    }
                }

                auto skybox = engine->reg.get_if<Skybox>(selected_entity);

                if (skybox)
                {
                    if (ImGui::CollapsingHeader("Skybox"))
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
                }

                auto forward = engine->reg.get_if<Forward>(selected_entity);

                if (forward)
                {
                    if (ImGui::CollapsingHeader("Forward")) ImGui::Checkbox("Transparent", &forward->transparent);
                }

                static char const* const components[] = { "Mesh", "Transform", "Material",
               "Visibility", "Light", "Skybox", "Deferred", "Forward" };

                static int current_component = 0;
                const char* combo_label = components[current_component];
                if (ImGui::BeginCombo("Components", combo_label, 0))
                {
                    for (int n = 0; n < IM_ARRAYSIZE(components); n++)
                    {
                        const bool is_selected = (current_component == n);
                        if (ImGui::Selectable(components[n], is_selected))
                            current_component = n;

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                enum COMPONENT_INDEX
                {
                    MESH,
                    TRANSFORM,
                    MATERIAL,
                    VISIBILITY,
                    LIGHT,
                    SKYBOX,
                    DEFERRED,
                    FORWARD
                };

                static model_parameters_t params{};
                if (current_component == MESH)
                {

                    if (ImGui::Button("Choose Mesh"))
                        ImGuiFileDialog::Instance()->OpenDialog("Choose Mesh", "Choose File", ".gltf", ".");

                    if (ImGuiFileDialog::Instance()->Display("Choose Mesh"))
                    {

                        if (ImGuiFileDialog::Instance()->IsOk())
                        {
                            std::string model_path = ImGuiFileDialog::Instance()->GetFilePathName();
                            params.model_path = model_path;
                        }

                        ImGuiFileDialog::Instance()->Close();
                    }
                }

                static LightType light_type = LightType::ePoint;
                if (current_component == LIGHT)
                {
                    static char const* const light_types[] = { "Directional", "Point", "Spot" };

                    static int current_light_type = 0;
                    ImGui::ListBox("Light Types", &current_light_type, light_types, IM_ARRAYSIZE(light_types));
                    light_type = static_cast<LightType>(current_light_type);
                }


                if (ImGui::Button("Add Component"))
                {
                    switch (current_component)
                    {
                    case MESH:
                        if (engine->reg.has<Mesh>(selected_entity))
                            Log::Warning("Entity already has Mesh Component!\n");
                        else
                            Log::Warning("Not supported for now!\n");
                        break;
                    case TRANSFORM:
                        if (engine->reg.has<Transform>(selected_entity))
                            Log::Warning("Entity already has Transform Component!\n");
                        else engine->reg.emplace<Transform>(selected_entity);
                        break;
                    case MATERIAL:
                        if (engine->reg.has<Material>(selected_entity))
                            Log::Warning("Entity already has Material Component!\n");
                        else
                        {
                            Material mat{};
                            if (engine->reg.has<Deferred>(selected_entity))
                                mat.pso = PSO::eGbufferPBR;
                            else mat.pso = PSO::eSolid;
                            engine->reg.emplace<Material>(selected_entity, mat);
                        }
                        break;
                    case VISIBILITY:
                        if (engine->reg.has<Visibility>(selected_entity))
                            Log::Warning("Entity already has Visibility Component!\n");
                        else if (!engine->reg.has<Mesh>(selected_entity) || !engine->reg.has<Transform>(selected_entity))
                        {
                            Log::Warning("Entity has to have Mesh and Transform Component before adding Visibility!\n");
                        }
                        else
                        {
                            auto& mesh = engine->reg.get<Mesh>(selected_entity);
                            auto& transform = engine->reg.get<Transform>(selected_entity);

                            BoundingBox aabb = AABBFromVertices(mesh.vertices);
                            aabb.Transform(aabb, transform.current_transform);

                            engine->reg.emplace<Visibility>(selected_entity, aabb, true, true);
                        }
                        break;
                    case LIGHT:
                        if (engine->reg.has<Light>(selected_entity))
                            Log::Warning("Entity already has Light Component!\n");
                        else
                        {
                            Light light{};
                            light.type = light_type;
                            engine->reg.emplace<Light>(selected_entity, light);
                        }
                        break;
                    case SKYBOX:
                        if (engine->reg.has<Skybox>(selected_entity))
                            Log::Warning("Entity already has Skybox Component!\n");
                        else  engine->reg.emplace<Skybox>(selected_entity);
                        break;
                    case DEFERRED:
                        if (engine->reg.has<Deferred>(selected_entity))
                            Log::Warning("Entity already has Deferred Component!\n");
                        else if (engine->reg.has<Forward>(selected_entity))
                            Log::Warning("Cannot add Deferred Component to entity that has Forward Component!\n");
                        else {
                            engine->reg.emplace<Deferred>(selected_entity);
                        }
                        break;
                    case FORWARD:
                        if (engine->reg.has<Forward>(selected_entity))
                            Log::Warning("Entity already has Forward Component!\n");
                        else if (engine->reg.has<Deferred>(selected_entity))
                            Log::Warning("Cannot add Forward Component to entity that has Deferred Component!\n");
                        else
                            engine->reg.emplace<Forward>(selected_entity, false);
                        break;
                    }
                }

                if (ImGui::Button("Remove Component"))
                {
                    switch (current_component)
                    {
                    case MESH:
                        if (!engine->reg.has<Mesh>(selected_entity))
                            Log::Warning("Entity doesn't have Mesh Component!\n");
                        else engine->reg.remove<Mesh>(selected_entity);
                        break;
                    case TRANSFORM:
                        if (!engine->reg.has<Transform>(selected_entity))
                            Log::Warning("Entity doesn't have Transform Component!\n");
                        else engine->reg.remove<Transform>(selected_entity);
                        break;
                    case MATERIAL:
                        if (!engine->reg.has<Material>(selected_entity))
                            Log::Warning("Entity doesn't have Material Component!\n");
                        else engine->reg.remove<Material>(selected_entity);
                        break;
                    case VISIBILITY:
                        if (!engine->reg.has<Visibility>(selected_entity))
                            Log::Warning("Entity doesn't have Visibility Component!\n");
                        else engine->reg.remove<Visibility>(selected_entity);
                        break;
                    case LIGHT:
                        if (!engine->reg.has<Light>(selected_entity))
                            Log::Warning("Entity doesn't have Light Component!\n");
                        else engine->reg.remove<Light>(selected_entity);
                        break;
                    case SKYBOX:
                        if (!engine->reg.has<Skybox>(selected_entity))
                            Log::Warning("Entity doesn't have Skybox Component!\n");
                        else engine->reg.remove<Skybox>(selected_entity);
                        break;
                    case DEFERRED:
                        if (!engine->reg.has<Deferred>(selected_entity))
                            Log::Warning("Entity doesn't have Deferred Component!\n");
                        else engine->reg.remove<Deferred>(selected_entity);
                        break;
                    case FORWARD:
                        if (!engine->reg.has<Forward>(selected_entity))
                            Log::Warning("Entity doesn't have Forward Component!\n");
                        else engine->reg.remove<Forward>(selected_entity);
                        break;
                    }
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
				f32 pos[3] = { camera.Position().m128_f32[0],camera.Position().m128_f32[1], camera.Position().m128_f32[2] };
				ImGui::SliderFloat3("Position", pos, 0.0f, 2000.0f);
				camera.SetPosition(DirectX::XMFLOAT3(pos));
				f32 _near = camera.Near(), _far = camera.Far();
				f32 _fov = camera.Fov(), _ar = camera.AspectRatio();
				ImGui::SliderFloat("Near Plane", &_near, 0.0f, 2.0f);
				ImGui::SliderFloat("Far Plane", &_far, 10.0f, 3000.0f);
				ImGui::SliderFloat("FOV", &_fov, 0.01f, 1.5707f);
				camera.SetNearAndFar(_near, _far);
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

            ImVec2 _scene_dimension = ImGui::GetWindowSize();
            
            D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetOffscreenTexture().SRV();
            OffsetType descriptor_index = descriptor_allocator->Allocate();
            D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
            device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            ImGui::Image((ImTextureID)descriptor_allocator->GetGpuHandle(descriptor_index).ptr, _scene_dimension);

            mouse_in_scene = ImGui::IsWindowFocused();
        }

        if (selected_entity != null_entity && engine->reg.has<Transform>(selected_entity) && gizmo_enabled)
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

            ImGuizmo::Manipulate(view.m[0], projection.m[0], gizmo_op, ImGuizmo::LOCAL,
                tr.m[0]);

            if (ImGuizmo::IsUsing())
            {
                Visibility* vis = engine->reg.get_if<Visibility>(selected_entity);

                if (vis)
                {
                    vis->aabb.Transform(vis->aabb, DirectX::XMMatrixInverse(nullptr, entity_transform.current_transform));
                    entity_transform.current_transform = DirectX::XMLoadFloat4x4(&tr);
                    vis->aabb.Transform(vis->aabb, entity_transform.current_transform);
                }
                else entity_transform.current_transform = DirectX::XMLoadFloat4x4(&tr);

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
                static const char* items[] = { "Regular", "Tiled", "Clustered" };
            
                static int item_current_idx = 0; 
                const char* combo_label = items[item_current_idx];  
                if (ImGui::BeginCombo("Deferred Type", combo_label, 0))
                {
                    for (int n = 0; n < IM_ARRAYSIZE(items); n++)
                    {
                        const bool is_selected = (item_current_idx == n);
                        if (ImGui::Selectable(items[n], is_selected))
                            item_current_idx = n;

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                renderer_settings.use_tiled_deferred = (item_current_idx == 1);
                renderer_settings.use_clustered_deferred = (item_current_idx == 2);

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
                    const char* items[] = { "None", "SSAO", "HBAO" };
                    static int item_current_idx = 0;
                    const char* combo_label = items[item_current_idx];
                    if (ImGui::BeginCombo("Ambient Occlusion", combo_label, 0))
                    {
                        for (int n = 0; n < IM_ARRAYSIZE(items); n++)
                        {
                            const bool is_selected = (item_current_idx == n);
                            if (ImGui::Selectable(items[n], is_selected))
                                item_current_idx = n;

                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    renderer_settings.ambient_occlusion = static_cast<AmbientOcclusion>(item_current_idx);

                    if (renderer_settings.ambient_occlusion == AmbientOcclusion::eSSAO && ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
                    {
                        //ImGui::Checkbox("SSAO", &settings.ssao);
                        ImGui::SliderFloat("Power", &renderer_settings.ssao_power, 1.0f, 16.0f);
                        ImGui::SliderFloat("Radius", &renderer_settings.ssao_radius, 0.5f, 4.0f);

                        ImGui::TreePop();
                        ImGui::Separator();
                    }
                    if (renderer_settings.ambient_occlusion == AmbientOcclusion::eHBAO && ImGui::TreeNodeEx("HBAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
                    {
                        //ImGui::Checkbox("SSAO", &settings.ssao);
                        ImGui::SliderFloat("Power", &renderer_settings.hbao_power, 1.0f, 16.0f);
                        ImGui::SliderFloat("Radius", &renderer_settings.hbao_radius, 0.25f, 8.0f);

                        ImGui::TreePop();
                        ImGui::Separator();
                    }
                }
                ImGui::Checkbox("Volumetric Clouds", &renderer_settings.clouds);
                ImGui::Checkbox("SSR", &renderer_settings.ssr);
                ImGui::Checkbox("DoF", &renderer_settings.dof);
                ImGui::Checkbox("Bloom", &renderer_settings.bloom);
                ImGui::Checkbox("Motion Blur", &renderer_settings.motion_blur);
                ImGui::Checkbox("Fog", &renderer_settings.fog);

                if (ImGui::TreeNode("Anti-Aliasing"))
                {
                    static bool fxaa = false, taa = false;
                    ImGui::Checkbox("FXAA", &fxaa);
                    ImGui::Checkbox("TAA", &taa);
                    if (fxaa)
                    {
                        renderer_settings.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.anti_aliasing | AntiAliasing_FXAA);
                    }
                    else
                    {
                        renderer_settings.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.anti_aliasing & (~AntiAliasing_FXAA));
                    }
                    if (taa)
                    {
                        renderer_settings.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.anti_aliasing | AntiAliasing_TAA);
                    }
                    else
                    {
                        renderer_settings.anti_aliasing = static_cast<AntiAliasing>(renderer_settings.anti_aliasing & (~AntiAliasing_TAA));
                    }

                    ImGui::TreePop();
                }
                if (renderer_settings.clouds && ImGui::TreeNodeEx("Volumetric Clouds", 0))
                {
                    ImGui::SliderFloat("Sun light absorption", &renderer_settings.light_absorption, 0.0f, 0.015f);
                    ImGui::SliderFloat("Clouds bottom height", &renderer_settings.clouds_bottom_height, 1000.0f, 10000.0f);
                    ImGui::SliderFloat("Clouds top height", &renderer_settings.clouds_top_height, 10000.0f, 50000.0f);
                    ImGui::SliderFloat("Density", &renderer_settings.density_factor, 0.0f, 1.0f);
                    ImGui::SliderFloat("Crispiness", &renderer_settings.crispiness, 0.0f, 100.0f);
                    ImGui::SliderFloat("Curliness", &renderer_settings.curliness, 0.0f, 5.0f);
                    ImGui::SliderFloat("Coverage", &renderer_settings.coverage, 0.0f, 1.0f);
                    ImGui::SliderFloat("Wind speed factor", &renderer_settings.wind_speed, 0.0f, 100.0f);
                    ImGui::SliderFloat("Cloud Type", &renderer_settings.cloud_type, 0.0f, 1.0f);
                    ImGui::TreePop();
                    ImGui::Separator();
                }
                if (renderer_settings.ssr && ImGui::TreeNodeEx("Screen-Space Reflections", 0))
                {
                    ImGui::SliderFloat("Ray Step", &renderer_settings.ssr_ray_step, 1.0f, 3.0f);
                    ImGui::SliderFloat("Ray Hit Threshold", &renderer_settings.ssr_ray_hit_threshold, 0.25f, 5.0f);

                    ImGui::TreePop();
                    ImGui::Separator();
                }
                if (renderer_settings.dof && ImGui::TreeNodeEx("Depth Of Field", 0))
                {

                    ImGui::SliderFloat("DoF Near Blur", &renderer_settings.dof_near_blur, 0.0f, 200.0f);
                    ImGui::SliderFloat("DoF Near", &renderer_settings.dof_near, renderer_settings.dof_near_blur, 500.0f);
                    ImGui::SliderFloat("DoF Far", &renderer_settings.dof_far, renderer_settings.dof_near, 1000.0f);
                    ImGui::SliderFloat("DoF Far Blur", &renderer_settings.dof_far_blur, renderer_settings.dof_far, 1500.0f);
                    ImGui::Checkbox("Bokeh", &renderer_settings.bokeh);

                    if (renderer_settings.bokeh)
                    {
                        static char const* const bokeh_types[] = { "HEXAGON", "OCTAGON", "CIRCLE", "CROSS" };
                        static int bokeh_type_i = static_cast<int>(renderer_settings.bokeh_type);
                        ImGui::ListBox("Bokeh Type", &bokeh_type_i, bokeh_types, IM_ARRAYSIZE(bokeh_types));
                        renderer_settings.bokeh_type = static_cast<BokehType>(bokeh_type_i);

                        ImGui::SliderFloat("Bokeh Blur Threshold", &renderer_settings.bokeh_blur_threshold, 0.0f, 1.0f);
                        ImGui::SliderFloat("Bokeh Lum Threshold", &renderer_settings.bokeh_lum_threshold, 0.0f, 10.0f);
                        ImGui::SliderFloat("Bokeh Color Scale", &renderer_settings.bokeh_color_scale, 0.1f, 10.0f);
                        ImGui::SliderFloat("Bokeh Max Size", &renderer_settings.bokeh_radius_scale, 0.0f, 100.0f);
                        ImGui::SliderFloat("Bokeh Fallout", &renderer_settings.bokeh_fallout, 0.0f, 2.0f);
                    }
                    
                    ImGui::TreePop();
                    ImGui::Separator();

                }
                if (renderer_settings.bloom && ImGui::TreeNodeEx("Bloom", 0))
                {
                    ImGui::SliderFloat("Threshold", &renderer_settings.bloom_threshold, 0.1f, 2.0f);
                    ImGui::SliderFloat("Bloom Scale", &renderer_settings.bloom_scale, 0.1f, 5.0f);
                    ImGui::TreePop();
                    ImGui::Separator();
                }
                if ((renderer_settings.motion_blur || (renderer_settings.anti_aliasing & AntiAliasing_TAA)) && ImGui::TreeNodeEx("Velocity Buffer", 0))
                {
                    ImGui::SliderFloat("Velocity Buffer Scale", &renderer_settings.velocity_buffer_scale, 32.0f, 128.0f);
                    ImGui::TreePop();
                    ImGui::Separator();
                }
                if (renderer_settings.fog && ImGui::TreeNodeEx("Fog", 0))
                {
                    const char* items[] = { "Exponential", "Exponential Height" };
                    static int item_current_idx = 0; // Here we store our selection data as an index.
                    const char* combo_label = items[item_current_idx];  // Label to preview before opening the combo (technically it could be anything)
                    if (ImGui::BeginCombo("Fog Type", combo_label, 0))
                    {
                        for (int n = 0; n < IM_ARRAYSIZE(items); n++)
                        {
                            const bool is_selected = (item_current_idx == n);
                            if (ImGui::Selectable(items[n], is_selected))
                                item_current_idx = n;

                            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    renderer_settings.fog_type = static_cast<FogType>(item_current_idx);

                    ImGui::SliderFloat("Fog Falloff", &renderer_settings.fog_falloff, 0.0001f, 0.01f);
                    ImGui::SliderFloat("Fog Density", &renderer_settings.fog_density, 0.0001f, 0.01f);
                    ImGui::SliderFloat("Fog Start", &renderer_settings.fog_start, 0.1f, 10000.0f);
                    ImGui::ColorEdit3("Fog Color", renderer_settings.fog_color);

                    ImGui::TreePop();
                    ImGui::Separator();
                }
                if (ImGui::TreeNodeEx("Tone Mapping", 0))
                {
                    ImGui::SliderFloat("Exposure", &renderer_settings.tonemap_exposure, 0.01f, 10.0f);
                    static char const* const operators[] = { "REINHARD", "HABLE", "LINEAR" };
                    static int tone_map_operator = static_cast<int>(renderer_settings.tone_map_op);
                    ImGui::ListBox("Tone Map Operator", &tone_map_operator, operators, IM_ARRAYSIZE(operators));
                    renderer_settings.tone_map_op = static_cast<ToneMap>(tone_map_operator);
                    ImGui::TreePop();
                    ImGui::Separator();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Misc"))
            {
                ImGui::ColorEdit3("Ambient Color", renderer_settings.ambient_color);
                ImGui::SliderFloat("Blur Sigma", &renderer_settings.blur_sigma, 0.1f, 10.0f);
                ImGui::SliderFloat("Shadow Softness", &renderer_settings.shadow_softness, 0.01f, 5.0f);
                
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
			ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			static bool enable_profiling = false;
			ImGui::Checkbox("Enable Profiling", &enable_profiling);
			//ImGui::PushTextWrapPos();
			if (enable_profiling)
			{
				ImGui::Checkbox("Profile GBuffer Pass", &profiler_settings.profile_gbuffer_pass);
				ImGui::Checkbox("Profile Deferred Pass", &profiler_settings.profile_deferred_pass);
				ImGui::Checkbox("Profile Forward Pass", &profiler_settings.profile_forward_pass);
				ImGui::Checkbox("Profile Postprocessing", &profiler_settings.profile_postprocessing);

				engine->renderer->SetProfilerSettings(profiler_settings);

				std::vector<std::string> results = engine->renderer->GetProfilerResults();
				std::string concatenated_results;
				for (auto const& result : results) concatenated_results += result;
				ImGui::TextWrapped(concatenated_results.c_str());
			}
			else
			{
				engine->renderer->SetProfilerSettings(NO_PROFILING);
			}
		}
		ImGui::End();
    }

    void Editor::OpenMaterialFileDialog(Material* material, MaterialTextureType type)
    {

        if (ImGuiFileDialog::Instance()->Display("Choose Texture"))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                std::wstring texture_path = ConvertToWide(ImGuiFileDialog::Instance()->GetFilePathName());

                switch (type)
                {
                case MaterialTextureType::eAlbedo:
                    material->albedo_texture = engine->renderer->GetTextureManager().LoadTexture(texture_path);
                    break;
                case MaterialTextureType::eMetallicRoughness:
                    material->metallic_roughness_texture = engine->renderer->GetTextureManager().LoadTexture(texture_path);
                    break;
                case MaterialTextureType::eEmissive:
                    material->emissive_texture = engine->renderer->GetTextureManager().LoadTexture(texture_path);
                    break;
                }
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }

}
