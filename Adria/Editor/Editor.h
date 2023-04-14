#pragma once
#include <memory>
#include <queue>
#include "GUICommand.h"
#include "EditorEvents.h"
#include "../Rendering/RendererSettings.h"
#include "../Rendering/ViewportData.h"
#include "../Utilities/Singleton.h"
#include "../ImGui/ImGuizmo.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class Engine;
	class GUI;
	class RenderGraph;
	class EditorLogger;
	class EditorConsole;
	struct EngineInit;
	struct Material;
	struct AABB;

	struct EditorInit
	{
		EngineInit& engine_init;
	};

	class Editor : public Singleton<Editor>
	{
		friend class Singleton<Editor>;

		enum
		{
			Flag_Profiler,
			Flag_Camera,
			Flag_Log,
			Flag_Console,
			Flag_Entities,
			Flag_HotReload,
			Flag_Debug,
			Flag_Settings,
			Flag_AddEntities,
			Flag_Count
		};

	public:
		
		void Init(EditorInit&& init);
		void Destroy();

		void HandleWindowMessage(WindowMessage const& msg_data);
		void Run();
		bool IsActive() const;

		void AddCommand(GUICommand&& command);
		void AddDebugCommand(GUICommand_Debug&& command);
		void AddRenderPass(RenderGraph& rg);

	private:
		std::unique_ptr<Engine> engine;
		std::unique_ptr<GUI> gui;
		GfxDevice* gfx;

		std::unique_ptr<EditorLogger> logger;
		std::unique_ptr<EditorConsole> console;

		bool scene_focused = false;
		entt::entity selected_entity;
		
		bool gizmo_enabled = false;
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
		void Console();
		void Settings();
		void Profiling();
		void ShaderHotReload();
		void Debug();
	};
	#define g_Editor Editor::Get()

}

