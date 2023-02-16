#pragma once
#include <memory>
#include "GUI.h"
#include "GUICommand.h"
#include "EditorEvents.h"
#include "../Core/Engine.h"
#include "../Rendering/RendererSettings.h"
#include "../Rendering/ViewportData.h"
#include "../ImGui/imgui_internal.h"
#include "../ImGui/ImGuizmo.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	struct Material;
	struct AABB;

	struct EditorInit
	{
		EngineInit engine_init;
	};
	class Editor
	{
		enum
		{
			Flag_Profiler,
			Flag_Camera,
			Flag_Log,
			Flag_Entities,
			Flag_HotReload,
			Flag_Debug,
			Flag_Settings,
			Flag_AddEntities,
			Flag_Count
		};

	public:
		static Editor& GetInstance()
		{
			static Editor editor;
			return editor;
		}
		void Init(EditorInit&& init);
		void Destroy();
		void HandleWindowMessage(WindowMessage const& msg_data);
		void Run();

		void AddCommand(GUICommand&& command);
		void AddDebugCommand(GUICommand_Debug&& command);
	private:
		std::unique_ptr<Engine> engine;
		std::unique_ptr<GUI> gui;
		std::unique_ptr<struct ImGuiLogger> editor_log;

		entt::entity selected_entity = entt::null;
		bool gizmo_enabled = false;
		bool scene_focused = false;
		ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;

		RendererSettings renderer_settings{};
		
		bool reload_shaders = false;
		std::queue<AABB*> aabb_updates;
		std::array<bool, Flag_Count> window_flags = { false };
		std::vector<GUICommand> commands;
		std::vector<GUICommand_Debug> debug_commands;

		EditorEvents editor_events;
		ViewportData viewport_data;

	private:
		Editor();
		~Editor();

		void SetStyle();
		void HandleInput();
		void MenuBar();
		void AddEntities();
		void ListEntities();
		void Properties();
		void Camera();
		void Scene();
		void Log();
		void Settings(); 
		void Profiling();
		void ShaderHotReload();
		void Debug();
	};
}

