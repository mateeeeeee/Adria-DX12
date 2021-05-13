#pragma once
#include "../Core/Engine.h"
#include <memory>
#include "GUI.h"
#include "../tecs/registry.h"
#include "../Rendering/RendererSettings.h"
#include "../ImGui/imgui_internal.h"
#include "../ImGui/ImGuizmo.h"


namespace adria
{

	struct editor_init_t
	{
		engine_init_t engine_init;
	};

	struct Material;

	class Editor
	{
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

	public:
		Editor(editor_init_t const& init);

		~Editor() = default;

		void HandleWindowMessage(window_message_t const& msg_data);

		void Run();

	private:
		std::unique_ptr<Engine> engine;
		std::unique_ptr<GUI> gui;
		EditorLogger editor_log;
		tecs::entity selected_entity = tecs::null_entity;
		bool gizmo_enabled = true;
		bool mouse_in_scene = false;
		ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;
		RendererSettings settings{};
	private:
		void SetStyle();

		void HandleInput();

		void MenuBar();

		void AddEntities();

		void ListEntities();

		void Properties();

		void Camera();

		void Scene();

		void Log();

		void RendererSettings();

		void StatsAndProfiling();

		void OpenMaterialFileDialog(Material* mat, MaterialTextureType type);
	};
}

