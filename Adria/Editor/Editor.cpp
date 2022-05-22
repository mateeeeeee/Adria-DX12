#include "Editor.h"

#include "../Rendering/Renderer.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Rendering/EntityLoader.h"
#include "../Rendering/RootSigPSOManager.h"
#include "../Logging/Logger.h"
#include "../Utilities/FilesUtil.h"
#include "../Utilities/StringUtil.h"
#include "../ImGui/ImGuiFileDialog.h"
#include "../Utilities/Random.h"
#include "../Math/BoundingVolumeHelpers.h"
#include "pix3.h"

using namespace DirectX;

namespace ImGui
{
	struct ComboFilterState
	{
		int  activeIdx;
		bool selectionChanged;
	};

	class ComboFilterCallback
	{
	public:
		virtual bool ComboFilterShouldOpenPopupCallback(const char* label, char* buffer, int bufferlen,
			const char** hints, int num_hints, ImGui::ComboFilterState* s)
		{
			return (buffer[0] != 0) && strcmp(buffer, hints[s->activeIdx]);
		}
	};

	bool ComboFilter(const char* label, char* buffer, int bufferlen, const char** hints, int num_hints, ComboFilterState& s, ComboFilterCallback* callback, ImGuiComboFlags flags = 0)
	{
		using namespace ImGui;

		s.selectionChanged = false;

		// Always consume the SetNextWindowSizeConstraint() call in our early return paths
		ImGuiContext& g = *GImGui;

		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		const ImGuiID popupId = window->GetID(label);
		bool popupIsAlreadyOpened = IsPopupOpen(popupId, 0); //ImGuiPopupFlags_AnyPopupLevel);
		bool popupNeedsToBeOpened = callback ? callback->ComboFilterShouldOpenPopupCallback(label, buffer, bufferlen, hints, num_hints, &s)
			: (buffer[0] != 0) && strcmp(buffer, hints[s.activeIdx]);
		bool popupJustOpened = false;

		IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

		const ImGuiStyle& style = g.Style;

		const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
		const ImVec2 label_size = CalcTextSize(label, NULL, true);
		const float expected_w = CalcItemWidth();
		const float w = (flags & ImGuiComboFlags_NoPreview) ? arrow_size : expected_w;
		const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w, window->DC.CursorPos.y + label_size.y + style.FramePadding.y * 2.0f));
		const ImRect total_bb(frame_bb.Min, ImVec2((label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f) + frame_bb.Max.x, frame_bb.Max.y));
		const float value_x2 = ImMax(frame_bb.Min.x, frame_bb.Max.x - arrow_size);
		ItemSize(total_bb, style.FramePadding.y);
		if (!ItemAdd(total_bb, popupId, &frame_bb))
			return false;


		bool hovered, held;
		bool pressed = ButtonBehavior(frame_bb, popupId, &hovered, &held);

		if (!popupIsAlreadyOpened) {
			const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
			RenderNavHighlight(frame_bb, popupId);
			if (!(flags & ImGuiComboFlags_NoPreview))
				window->DrawList->AddRectFilled(frame_bb.Min, ImVec2(value_x2, frame_bb.Max.y), frame_col, style.FrameRounding, (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
		}
		if (!(flags & ImGuiComboFlags_NoArrowButton))
		{
			ImU32 bg_col = GetColorU32((popupIsAlreadyOpened || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
			ImU32 text_col = GetColorU32(ImGuiCol_Text);
			window->DrawList->AddRectFilled(ImVec2(value_x2, frame_bb.Min.y), frame_bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight);
			if (value_x2 + arrow_size - style.FramePadding.x <= frame_bb.Max.x)
				RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, frame_bb.Min.y + style.FramePadding.y), text_col, ImGuiDir_Down, 1.0f);
		}

		if (!popupIsAlreadyOpened)
		{
			RenderFrameBorder(frame_bb.Min, frame_bb.Max, style.FrameRounding);
			if (buffer != NULL && !(flags & ImGuiComboFlags_NoPreview))

				RenderTextClipped(ImVec2(frame_bb.Min.x + style.FramePadding.x, frame_bb.Min.y + style.FramePadding.y), ImVec2(value_x2, frame_bb.Max.y), buffer, NULL, NULL, ImVec2(0.0f, 0.0f));

			if ((pressed || g.NavActivateId == popupId || popupNeedsToBeOpened) && !popupIsAlreadyOpened)
			{
				if (window->DC.NavLayerCurrent == 0)
					window->NavLastIds[0] = popupId;
				OpenPopupEx(popupId);
				popupIsAlreadyOpened = true;
				popupJustOpened = true;
			}
		}

		if (label_size.x > 0)
		{
			RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);
		}

		if (!popupIsAlreadyOpened) {
			return false;
		}

		const float totalWMinusArrow = w - arrow_size;
		struct ImGuiSizeCallbackWrapper {
			static void sizeCallback(ImGuiSizeCallbackData* data)
			{
				float* totalWMinusArrow = (float*)(data->UserData);
				data->DesiredSize = ImVec2(*totalWMinusArrow, 200.f);
			}
		};
		SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(totalWMinusArrow, 150.f), ImGuiSizeCallbackWrapper::sizeCallback, (void*)&totalWMinusArrow);

		char name[16];
		ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginPopupStack.Size); // Recycle windows based on depth

		// Peek into expected window size so we can position it
		if (ImGuiWindow* popup_window = FindWindowByName(name))
		{
			if (popup_window->WasActive)
			{
				ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
				if (flags & ImGuiComboFlags_PopupAlignLeft)
					popup_window->AutoPosLastDirection = ImGuiDir_Left;
				ImRect r_outer = GetWindowAllowedExtentRect(popup_window);
				ImVec2 pos = FindBestWindowPosForPopupEx(frame_bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, frame_bb, ImGuiPopupPositionPolicy_ComboBox);

				pos.y -= label_size.y + style.FramePadding.y * 2.0f;

				SetNextWindowPos(pos);
			}
		}

		// Horizontally align ourselves with the framed text
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
		//    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, style.WindowPadding.y));
		bool ret = Begin(name, NULL, window_flags);

		ImGui::PushItemWidth(ImGui::GetWindowWidth());
		ImGui::SetCursorPos(ImVec2(0.f, window->DC.CurrLineTextBaseOffset));
		if (popupJustOpened)
		{
			ImGui::SetKeyboardFocusHere(0);
		}

		bool done = InputTextEx("##inputText", NULL, buffer, bufferlen, ImVec2(0, 0),
			ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue,
			NULL, NULL);
		ImGui::PopItemWidth();

		if (s.activeIdx < 0) {
			IM_ASSERT(false); //Undefined behaviour
			return false;
		}


		if (!ret)
		{
			ImGui::EndChild();
			ImGui::PopItemWidth();
			EndPopup();
			IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
			return false;
		}


		ImGuiWindowFlags window_flags2 = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus; //0; //ImGuiWindowFlags_HorizontalScrollbar
		ImGui::BeginChild("ChildL", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), false, window_flags2);

		struct fuzzy {
			static int score(const char* str1, const char* str2) {
				int score = 0, consecutive = 0, maxerrors = 0;
				while (*str1 && *str2) {
					int is_leading = (*str1 & 64) && !(str1[1] & 64);
					if ((*str1 & ~32) == (*str2 & ~32)) {
						int had_separator = (str1[-1] <= 32);
						int x = had_separator || is_leading ? 10 : consecutive * 5;
						consecutive = 1;
						score += x;
						++str2;
					}
					else {
						int x = -1, y = is_leading * -3;
						consecutive = 0;
						score += x;
						maxerrors += y;
					}
					++str1;
				}
				return score + (maxerrors < -9 ? -9 : maxerrors);
			}
			static int search(const char* str, int num, const char* words[]) {
				int scoremax = 0;
				int best = -1;
				for (int i = 0; i < num; ++i) {
					int score = fuzzy::score(words[i], str);
					int record = (score >= scoremax);
					int draw = (score == scoremax);
					if (record) {
						scoremax = score;
						if (!draw) best = i;
						else best = best >= 0 && strlen(words[best]) < strlen(words[i]) ? best : i;
					}
				}
				return best;
			}
		};

		bool selectionChangedLocal = false;
		if (buffer[0] != '\0')
		{
			int new_idx = fuzzy::search(buffer, num_hints, hints);
			int idx = new_idx >= 0 ? new_idx : s.activeIdx;
			s.selectionChanged = s.activeIdx != idx;
			selectionChangedLocal = s.selectionChanged;
			s.activeIdx = idx;
		}

		bool arrowScroll = false;
		int arrowScrollIdx = s.activeIdx;

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
		{
			if (s.activeIdx > 0)
			{
				s.activeIdx--;
				//			selectionChangedLocal = true;
				arrowScroll = true;
				ImGui::SetWindowFocus();
				//			arrowScrollIdx = s.activeIdx;
			}
		}
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
		{
			if (s.activeIdx < num_hints - 1)
			{
				s.activeIdx++;
				//			selectionChangedLocal = true;
				arrowScroll = true;
				ImGui::SetWindowFocus();
				//			arrowScrollIdx = s.activeIdx;
			}
		}

		if (done && !arrowScroll) {
			CloseCurrentPopup();
		}

		bool done2 = false;

		for (int n = 0; n < num_hints; n++)
		{
			bool is_selected = n == s.activeIdx;
			if (is_selected && (IsWindowAppearing() || selectionChangedLocal))
			{
				SetScrollHereY();
				//            ImGui::SetItemDefaultFocus();
			}

			if (is_selected && arrowScroll)
			{
				SetScrollHereY();
			}

			if (ImGui::Selectable(hints[n], is_selected))
			{
				s.selectionChanged = s.activeIdx != n;
				s.activeIdx = n;
				strcpy(buffer, hints[n]);
				CloseCurrentPopup();

				done2 = true;
			}
		}

		if (arrowScroll)
		{
			strcpy(buffer, hints[s.activeIdx]);
			ImGuiWindow* window = FindWindowByName(name);
			const ImGuiID id = window->GetID("##inputText");
			ImGuiInputTextState* state = GetInputTextState(id);

			const char* buf_end = NULL;
			state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, buffer, NULL, &buf_end);
			state->CurLenA = (int)(buf_end - buffer);
			state->CursorClamp();
		}

		ImGui::EndChild();
		EndPopup();

		bool ret1 = (s.selectionChanged && !strcmp(hints[s.activeIdx], buffer));

		bool widgetRet = done || done2 || ret1;

		return widgetRet;
	}
}

namespace adria
{
    using namespace tecs;

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
            engine->SetSceneViewportData(scene_viewport_data);
            engine->Run(renderer_settings);
            auto gui_cmd_list = engine->gfx->GetNewGraphicsCommandList();
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
				if (engine->renderer->IsRayTracingSupported()) RayTracingDebug();
                gui->End(gui_cmd_list);
            }
            engine->Present();
        }
        else
        {
            engine->SetSceneViewportData(std::nullopt);
            engine->Run(renderer_settings);
            engine->Present();
        }

		if (shader_reload_callback != nullptr)
		{
			shader_reload_callback();
			shader_reload_callback = nullptr;
		}
    }

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
			static grid_parameters_t ocean_params{};
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
				ocean_parameters_t params{};
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
			static emitter_parameters_t params{};
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
				engine->entity_loader->LoadEmitter(params);
			}
		}
		ImGui::End();
	}

	void Editor::DecalSettings()
	{
		ImGui::Begin("Decals");
		{
			static decal_parameters_t params{};
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
				engine->reg.destroy<Decal>();
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
                        light_parameters_t light_params{};
                        light_params.light_data.casts_shadows = false;
                        light_params.light_data.color = DirectX::XMVectorSet(real() * 2, real() * 2, real() * 2, 1.0f);
                        light_params.light_data.direction = DirectX::XMVectorSet(0.5f, -1.0f, 0.1f, 0.0f);
                        light_params.light_data.position = DirectX::XMVectorSet(real() * 500 - 250, real() * 500.0f, real() * 500 - 250, 1.0f);
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

                    if (engine->reg.has<Material>(selected_entity))
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

                    if (engine->reg.has<Transform>(selected_entity))
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

                auto material = engine->reg.get_if<Material>(selected_entity);
                if (material && ImGui::CollapsingHeader("Material"))
                {
                    ID3D12Device5* device = engine->gfx->GetDevice();
                    RingDescriptorAllocator* descriptor_allocator = gui->DescriptorAllocator();

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
                    if (engine->reg.has<Forward>(selected_entity))
                    {
                        if (material->albedo_texture != INVALID_TEXTURE_HANDLE)
                            material->pso = EPipelineStateObject::Texture;
                        else material->pso = EPipelineStateObject::Solid;
                    }
                    else
                    {
                        material->pso = EPipelineStateObject::GbufferPBR;
                    }
                }

                auto transform = engine->reg.get_if<Transform>(selected_entity);
                if (transform && ImGui::CollapsingHeader("Transform"))
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

				auto emitter = engine->reg.get_if<Emitter>(selected_entity);
				if (emitter && ImGui::CollapsingHeader("Emitter"))
				{
					ID3D12Device5* device = engine->gfx->GetDevice();
					RingDescriptorAllocator* descriptor_allocator = gui->DescriptorAllocator();

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

				auto decal = engine->reg.get_if<Decal>(selected_entity);
				if (decal && ImGui::CollapsingHeader("Decal"))
				{
					ID3D12Device5* device = engine->gfx->GetDevice();
					RingDescriptorAllocator* descriptor_allocator = gui->DescriptorAllocator();

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

                auto skybox = engine->reg.get_if<Skybox>(selected_entity);
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

                auto forward = engine->reg.get_if<Forward>(selected_entity);
                if (forward)
                {
                    if (ImGui::CollapsingHeader("Forward")) ImGui::Checkbox("Transparent", &forward->transparent);
                }

                static char const* const components[] = { "Mesh", "Transform", "Material",
               "Visibility", "Light", "Skybox", "Deferred", "Forward", "Emitter" };

                static int current_component = 0;
                const char* combo_label = components[current_component];
                if (ImGui::BeginCombo("Components", combo_label, 0))
                {
                    for (int n = 0; n < IM_ARRAYSIZE(components); n++)
                    {
                        const bool is_selected = (current_component == n);
                        if (ImGui::Selectable(components[n], is_selected))
                            current_component = n;

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                enum EComponentIndex
                {
                    MESH,
                    TRANSFORM,
                    MATERIAL,
                    VISIBILITY,
                    LIGHT,
                    SKYBOX,
                    DEFERRED,
                    FORWARD,
                    EMITTER
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

                static ELightType light_type = ELightType::Point;
                if (current_component == LIGHT)
                {
                    static char const* const light_types[] = { "Directional", "Point", "Spot" };

                    static int current_light_type = 0;
                    ImGui::ListBox("Light Types", &current_light_type, light_types, IM_ARRAYSIZE(light_types));
                    light_type = static_cast<ELightType>(current_light_type);
                }

                if (ImGui::Button("Add Component"))
                {
                    switch (current_component)
                    {
					case MESH:
						if (engine->reg.has<Mesh>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Mesh Component!");
						}
						else
						{
							ADRIA_LOG(WARNING, "Not supported for now!");
						}
						break;
					case TRANSFORM:
						if (engine->reg.has<Transform>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Transform Component!");
						}
						else engine->reg.emplace<Transform>(selected_entity);
						break;
					case MATERIAL:
						if (engine->reg.has<Material>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Material Component!");
						}
						else
						{
							Material mat{};
							if (engine->reg.has<Deferred>(selected_entity))
								mat.pso = EPipelineStateObject::GbufferPBR;
							else mat.pso = EPipelineStateObject::Solid;
							engine->reg.emplace<Material>(selected_entity, mat);
						}
						break;
					case VISIBILITY:
						if (engine->reg.has<Visibility>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Visibility Component!");
						}
						else if (!engine->reg.has<Mesh>(selected_entity) || !engine->reg.has<Transform>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity has to have Mesh and Transform Component before adding Visibility!");
						}
						break;
					case LIGHT:
						if (engine->reg.has<Light>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Light Component!");
						}
						else
						{
							Light light{};
							light.type = light_type;
							engine->reg.emplace<Light>(selected_entity, light);
						}
						break;
					case SKYBOX:
						if (engine->reg.has<Skybox>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Skybox Component!");
						}
						else
						{
							engine->reg.emplace<Skybox>(selected_entity);
						}
						break;
					case DEFERRED:
						if (engine->reg.has<Deferred>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Deferred Component!");
						}
						else if (engine->reg.has<Forward>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Cannot add Deferred Component to entity that has Forward Component!");
						}
						else
						{
							engine->reg.emplace<Deferred>(selected_entity);
						}
						break;
					case FORWARD:
						if (engine->reg.has<Forward>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Forward Component!");
						}
						else if (engine->reg.has<Deferred>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Cannot add Forward Component to entity that has Deferred Component!");
						}
						else
						{
							engine->reg.emplace<Forward>(selected_entity, false);
						}
						break;
					case EMITTER:
						if (engine->reg.has<Emitter>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity already has Emitter Component!");
						}
						else
						{
							engine->reg.emplace<Emitter>(selected_entity);
						}
                    }
                }

                if (ImGui::Button("Remove Component"))
                {
					switch (current_component)
					{
					case MESH:
						if (!engine->reg.has<Mesh>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Mesh Component!");
						}
						else
						{
							engine->reg.remove<Mesh>(selected_entity);
						}
						break;
					case TRANSFORM:
						if (!engine->reg.has<Transform>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Transform Component!");
						}
						else
						{
							engine->reg.remove<Transform>(selected_entity);
						}
						break;
					case MATERIAL:
						if (!engine->reg.has<Material>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Material Component!");
						}
						else
						{
							engine->reg.remove<Material>(selected_entity);
						}
						break;
					case VISIBILITY:
						if (!engine->reg.has<Visibility>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Visibility Component!");
						}
						else
						{
							engine->reg.remove<Visibility>(selected_entity);
						}
						break;
					case LIGHT:
						if (!engine->reg.has<Light>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Light Component!");
						}
						else
						{
							engine->reg.remove<Light>(selected_entity);
						}
						break;
					case SKYBOX:
						if (!engine->reg.has<Skybox>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Skybox Component!");
						}
						else
						{
							engine->reg.remove<Skybox>(selected_entity);
						}
						break;
					case DEFERRED:
						if (!engine->reg.has<Deferred>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Deferred Component!");
						}
						else
						{
							engine->reg.remove<Deferred>(selected_entity);
						}
						break;
					case FORWARD:
						if (!engine->reg.has<Forward>(selected_entity))
						{
							ADRIA_LOG(WARNING, "Entity doesn't have Forward Component!");
						}
						else
						{
							engine->reg.remove<Forward>(selected_entity);
						}
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

            D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetOffscreenTexture().SRV();
            OffsetType descriptor_index = descriptor_allocator->Allocate();
            auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
            device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);

            scene_focused = ImGui::IsWindowFocused();

			ImVec2 mouse_pos = ImGui::GetMousePos();
			scene_viewport_data.mouse_position_x = mouse_pos.x;
			scene_viewport_data.mouse_position_y = mouse_pos.y;
			scene_viewport_data.scene_viewport_focused = scene_focused;
			scene_viewport_data.scene_viewport_pos_x = v_min.x;
			scene_viewport_data.scene_viewport_pos_y = v_min.y;
			scene_viewport_data.scene_viewport_size_x = size.x;
			scene_viewport_data.scene_viewport_size_y = size.y;
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

                    renderer_settings.ambient_occlusion = static_cast<EAmbientOcclusion>(current_ao_type);

                    if (renderer_settings.ambient_occlusion == EAmbientOcclusion::SSAO && ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
                    {
                        ImGui::SliderFloat("Power", &renderer_settings.ssao_power, 1.0f, 16.0f);
                        ImGui::SliderFloat("Radius", &renderer_settings.ssao_radius, 0.5f, 4.0f);

                        ImGui::TreePop();
                        ImGui::Separator();
                    }
					else if (renderer_settings.ambient_occlusion == EAmbientOcclusion::HBAO && ImGui::TreeNodeEx("HBAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
                    {
                        ImGui::SliderFloat("Power", &renderer_settings.hbao_power, 1.0f, 16.0f);
                        ImGui::SliderFloat("Radius", &renderer_settings.hbao_radius, 0.25f, 8.0f);

                        ImGui::TreePop();
                        ImGui::Separator();
                    }
					else if (renderer_settings.ambient_occlusion == EAmbientOcclusion::RTAO && ImGui::TreeNodeEx("RTAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
					{
						ImGui::SliderFloat("Radius", &renderer_settings.rtao_radius, 0.25f, 8.0f);

						ImGui::TreePop();
						ImGui::Separator();
					}
                }
				//reflections
				{
					static const char* reflection_types[] = { "None", "SSR", "RTR"};
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

					renderer_settings.reflections = static_cast<EReflections>(current_reflection_type);
					if (renderer_settings.reflections == EReflections::SSR && ImGui::TreeNodeEx("Screen-Space Reflections", 0))
					{
						ImGui::SliderFloat("Ray Step", &renderer_settings.ssr_ray_step, 1.0f, 3.0f);
						ImGui::SliderFloat("Ray Hit Threshold", &renderer_settings.ssr_ray_hit_threshold, 0.25f, 5.0f);
					
						ImGui::TreePop();
						ImGui::Separator();
					}
				}
                ImGui::Checkbox("Volumetric Clouds", &renderer_settings.clouds);
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
                        renderer_settings.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.anti_aliasing | AntiAliasing_FXAA);
                    }
                    else
                    {
                        renderer_settings.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.anti_aliasing & (~AntiAliasing_FXAA));
                    }
                    if (taa)
                    {
                        renderer_settings.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.anti_aliasing | AntiAliasing_TAA);
                    }
                    else
                    {
                        renderer_settings.anti_aliasing = static_cast<EAntiAliasing>(renderer_settings.anti_aliasing & (~AntiAliasing_TAA));
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
                        renderer_settings.bokeh_type = static_cast<EBokehType>(bokeh_type_i);

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

					renderer_settings.fog_type = static_cast<EFogType>(current_fog_type);

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
                    renderer_settings.tone_map_op = static_cast<EToneMap>(tone_map_operator);
                    ImGui::TreePop();
                    ImGui::Separator();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Misc"))
            {
				ImGui::SliderFloat2("Wind Direction", renderer_settings.wind_direction, 0.0f, 50.0f);
				ImGui::SliderFloat("Wind speed factor", &renderer_settings.wind_speed, 0.0f, 100.0f);
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
			ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			static bool enable_profiling = false;
			ImGui::Checkbox("Enable Profiling", &enable_profiling);
			if (enable_profiling)
			{
                static bool log_results = false;
                ImGui::Checkbox("Log Results", &log_results);
				ImGui::Checkbox("Profile GBuffer Pass", &profiler_settings.profile_gbuffer_pass);
				ImGui::Checkbox("Profile Decal Pass", &profiler_settings.profile_decal_pass);
				ImGui::Checkbox("Profile Deferred Pass", &profiler_settings.profile_deferred_pass);
				ImGui::Checkbox("Profile Forward Pass", &profiler_settings.profile_forward_pass);
				ImGui::Checkbox("Profile Particles Pass", &profiler_settings.profile_particles_pass);
				ImGui::Checkbox("Profile Postprocessing", &profiler_settings.profile_postprocessing);

				ImGui::Checkbox("Profile Ray Traced Shadows", &profiler_settings.profile_rts);
				ImGui::Checkbox("Profile Ray Traced AO", &profiler_settings.profile_rtao);
				ImGui::Checkbox("Profile Ray Traced Reflections", &profiler_settings.profile_rtr);

				engine->renderer->SetProfilerSettings(profiler_settings);

				std::vector<std::string> results = engine->renderer->GetProfilerResults(log_results);
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

	void Editor::ShaderHotReload()
	{
		if (ImGui::Begin("Shader Hot Reload"))
		{
			static ImGui::ComboFilterState s = { 0, false };
			static char buf[128];
			static bool once = false;
			if (!once)
			{
				memcpy(buf, ShaderNames[0], strlen(ShaderNames[0]) + 1);
				once = true;
			}
			ComboFilter("Shader", buf, IM_ARRAYSIZE(buf), ShaderNames, IM_ARRAYSIZE(ShaderNames), s, NULL);

			if (ImGui::Button("Compile Shader"))
			{
				engine->gfx->WaitForGPU();
				shader_reload_callback = []() {RootSigPSOManager::RecompileShader((EShader)s.activeIdx); };
			}

			if (ImGui::Button("Compile Changed Shaders"))
			{
				engine->gfx->WaitForGPU();
				shader_reload_callback = RootSigPSOManager::RecompileChangedShaders;
			}
			if (ImGui::Button("Compile All Shaders"))
			{
				engine->gfx->WaitForGPU();
				shader_reload_callback = RootSigPSOManager::RecompileAllShaders;
			}
		}
		ImGui::End();
	}

	void Editor::RayTracingDebug()
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

		ImGui::Begin("Ray Tracing Debug");
		{
			static const char* rt_types[] = { "Shadows", "Ambient Occlusion", "Reflections"};
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
				D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetRayTracingShadowsTexture_Debug().SRV();
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);
				ImGui::Text("Ray Tracing Shadows Image");
			}
			else if (current_rt_type == 1)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetRayTracingAOTexture_Debug().SRV();
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);
				ImGui::Text("Ray Tracing AO Image");
			}
			else if (current_rt_type == 2)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE tex_handle = engine->renderer->GetRayTracingReflectionsTexture_Debug().SRV();
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, tex_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				ImGui::Image((ImTextureID)static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(dst_descriptor).ptr, size);
				ImGui::Text("Ray Tracing Reflections Image");
			}

		}
		ImGui::End();
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
