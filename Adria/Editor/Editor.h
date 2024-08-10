#pragma once
#include <memory>
#include <queue>
#include "GUICommand.h"
#include "EditorEvents.h"
#include "Rendering/ViewportData.h"
#include "Utilities/Singleton.h"
#include "ImGui/ImGuizmo.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class GfxDescriptor;
	class Engine;
	class ImGuiManager;
	class RenderGraph;
	class EditorLogger;
	class EditorConsole;
	struct EngineInit;
	struct Material;

	struct EditorInit
	{
		EngineInit& engine_init;
	};

	class Editor : public Singleton<Editor>
	{
		friend class Singleton<Editor>;

		enum VisibilityFlag
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

		void OnWindowEvent(WindowEventData const& msg_data);
		void Run();
		bool IsActive() const;

		void AddCommand(GUICommand&& command);
		void AddDebugTexture(GUITexture&& debug_texture);
		void AddRenderPass(RenderGraph& rg);

	private:
		std::unique_ptr<Engine> engine;
		std::unique_ptr<ImGuiManager> gui;
		GfxDevice* gfx;
		bool ray_tracing_supported = false;

		std::unique_ptr<EditorConsole> console;
		EditorLogger* logger;

		bool scene_focused = false;
		entt::entity selected_entity;
		
		bool gizmo_enabled = false;
		ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;

		bool reload_shaders = false;
		bool visibility_flags[Flag_Count] = {false};
		std::vector<GUICommand> commands;
		std::vector<GUITexture> debug_textures;
		
		EditorEvents editor_events;
		ViewportData viewport_data;
		bool show_basic_console = false;

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
		void Scene(GfxDescriptor& tex);
		void Log();
		void Console();
		void Settings();
		void Profiling();
		void ShaderHotReload();
		void Debug();
	};
	#define g_Editor Editor::Get()

}

