#include "EditorConsole.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static int Stricmp(const char* s1, const char* s2)
	{
		int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d;
	}
	static int Strnicmp(const char* s1, const char* s2, int n)
	{
		int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d;
	}
	static char* Strdup(const char* s)
	{
		IM_ASSERT(s); uint64 len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len);
	}
	static void  Strtrim(char* s)
	{
		char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0;
	}

	EditorConsole::EditorConsole()
	{
		ClearLog();
		memset(InputBuf, 0, sizeof(InputBuf));
		HistoryPos = -1;

		Commands.push_back("help");
		CommandDescriptions.push_back("Displays all the possible commands");
		Commands.push_back("history");
		CommandDescriptions.push_back("Shows the previous commands used");
		Commands.push_back("clear");
		CommandDescriptions.push_back("Clear the history");
		g_ConsoleManager.ForAllObjects(ConsoleObjectDelegate::CreateLambda([&](IConsoleObject* const cobj)
			{
				Commands.push_back(cobj->GetName());
				CommandDescriptions.push_back(cobj->GetHelp());
			}
		));
		AutoScroll = true;
		ScrollToBottom = false;
	}
	EditorConsole::~EditorConsole()
	{
		ClearLog();
		for (int i = 0; i < History.Size; i++) free(History[i]);
	}

	void EditorConsole::Draw(const char* title, bool* p_open)
	{
		ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title, p_open))
		{
			ImGui::End();
			return;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear")) { ClearLog(); }
		ImGui::SameLine();
		bool copy_to_clipboard = ImGui::SmallButton("Copy");

		ImGui::Separator();

		if (ImGui::BeginPopup("Options"))
		{
			ImGui::Checkbox("Auto-scroll", &AutoScroll);
			ImGui::EndPopup();
		}

		if (ImGui::Button("Options")) ImGui::OpenPopup("Options");
		ImGui::SameLine();
		Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
		ImGui::Separator();

		// Reserve enough left-over height for 1 separator + 1 input text
		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::Selectable("Clear")) ClearLog();
			ImGui::EndPopup();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		if (copy_to_clipboard) ImGui::LogToClipboard();
		for (int i = 0; i < Items.Size; i++)
		{
			const char* item = Items[i];
			if (!Filter.PassFilter(item)) continue;
			ImGui::TextUnformatted(item);
		}
		if (copy_to_clipboard) ImGui::LogFinish();

		if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;

		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::Separator();

		auto TextEditCallbackStub = [](ImGuiInputTextCallbackData* data)
			{
				EditorConsole* console = (EditorConsole*)data->UserData;
				return console->TextEditCallback(data);
			};
		// Command-line
		bool reclaim_focus = false;
		ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, TextEditCallbackStub, (void*)this))
		{
			char* s = InputBuf;
			Strtrim(s);
			if (s[0]) ExecCommand(s);
			strcpy(s, "");
			reclaim_focus = true;
		}

		// Auto-focus on window apparition
		ImGui::SetItemDefaultFocus();
		if (reclaim_focus)
			ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

		ImGui::End();
	}

	void EditorConsole::DrawBasic(const char* title, bool* p_open)
	{
		ImGui::SetNextWindowBgAlpha(0.5f);

		if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration))
		{
			ImGui::End();
			return;
		}

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
			ImGui::SetKeyboardFocusHere(0);

		auto TextEditCallbackStub = [](ImGuiInputTextCallbackData* data)
			{
				EditorConsole* console = (EditorConsole*)data->UserData;
				console->CursorPos = data->CursorPos;
				return console->TextEditCallback(data);
			};

		ImGui::TextUnformatted("> ");
		ImGui::SameLine();

		// Save current style
		ImVec4 inputBgColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
		ImVec4 inputBgHoveredColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered);
		ImVec4 inputBgActiveColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);
		ImVec4 borderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

		ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
		ImGui::PushItemWidth(-1);
		bool reclaim_focus = false;
		if (ImGui::InputText("##console", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, TextEditCallbackStub, (void*)this))
		{
			char* s = InputBuf;
			Strtrim(s);
			if (s[0]) ExecCommand(s);
			strcpy(s, "");
			ImGui::SetItemDefaultFocus();
			ImGui::SetKeyboardFocusHere(-1);
		}
		ImGui::PopItemWidth();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);

		ImGui::SameLine();
		ImGui::TextUnformatted(InputBuf);

		for (int i = History.Size - 1; i >= 0 ; i--)
		{
			ImGui::TextUnformatted(History[i]);
		}
		
		ImGui::End();
	}

	void EditorConsole::ClearLog()
	{
		for (int i = 0; i < Items.Size; i++) free(Items[i]);
		Items.clear();
		for (int i = 0; i < History.Size; i++) free(History[i]);
		History.clear();
	}
	void EditorConsole::AddLog(const char* fmt, ...) IM_FMTARGS(2)
	{
		// FIXME-OPT
		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
		buf[IM_ARRAYSIZE(buf) - 1] = 0;
		va_end(args);
		Items.push_back(Strdup(buf));
	}
	void EditorConsole::ExecCommand(const char* cmd)
	{
		AddLog("# %s\n", cmd);
		HistoryPos = -1;
		for (int i = History.Size - 1; i >= 0; i--)
		{
			if (Stricmp(History[i], cmd) == 0)
			{
				free(History[i]);
				History.erase(History.begin() + i);
				break;
			}
		}
		History.push_back(Strdup(cmd));

		if (Stricmp(cmd, "clear") == 0)
		{
			ClearLog();
		}
		else if (Stricmp(cmd, "help") == 0)
		{
			AddLog("Commands:");
			for (int i = 0; i < Commands.Size; i++) AddLog("- %s : %s", Commands[i], CommandDescriptions[i]);
		}
		else if (Stricmp(cmd, "history") == 0)
		{
			int first = History.Size - 10;
			for (int i = first > 0 ? first : 0; i < History.Size; i++)
				AddLog("%3d: %s\n", i, History[i]);
		}
		else if (!g_ConsoleManager.ProcessInput(cmd))
		{
			AddLog("Unknown command: '%s'\n", cmd);
		}

		ScrollToBottom = true;
	}
	int EditorConsole::TextEditCallback(ImGuiInputTextCallbackData* data)
	{
		switch (data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
		{
			const char* word_end = data->Buf + data->CursorPos;
			const char* word_start = word_end;
			while (word_start > data->Buf)
			{
				const char c = word_start[-1];
				if (c == ' ' || c == '\t' || c == ',' || c == ';')
					break;
				word_start--;
			}

			ImVector<const char*> candidates;
			for (int i = 0; i < Commands.Size; i++)
				if (Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
					candidates.push_back(Commands[i]);

			if (candidates.Size == 0)
			{
				AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
			}
			else if (candidates.Size == 1)
			{
				data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
				data->InsertChars(data->CursorPos, candidates[0]);
				data->InsertChars(data->CursorPos, " ");
			}
			else if (candidates.Size != Commands.Size)
			{
				int match_len = (int)(word_end - word_start);
				for (;;)
				{
					int c = 0;
					bool all_candidates_matches = true;
					for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
						if (i == 0)
							c = toupper(candidates[i][match_len]);
						else if (c == 0 || c != toupper(candidates[i][match_len]))
							all_candidates_matches = false;
					if (!all_candidates_matches)
						break;
					match_len++;
				}

				if (match_len > 0)
				{
					data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
					data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
				}

				// List matches
				AddLog("Possible matches:\n");
				for (int i = 0; i < candidates.Size; i++)
					AddLog("- %s\n", candidates[i]);
			}

			break;
		}
		case ImGuiInputTextFlags_CallbackHistory:
		{
			const int prev_history_pos = HistoryPos;
			if (data->EventKey == ImGuiKey_UpArrow)
			{
				if (HistoryPos == -1)
					HistoryPos = History.Size - 1;
				else if (HistoryPos > 0)
					HistoryPos--;
			}
			else if (data->EventKey == ImGuiKey_DownArrow)
			{
				if (HistoryPos != -1)
					if (++HistoryPos >= History.Size)
						HistoryPos = -1;
			}

			// A better implementation would preserve the data on the current input line along with cursor position.
			if (prev_history_pos != HistoryPos)
			{
				const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, history_str);
			}
		}
		}
		return 0;
	}
}

