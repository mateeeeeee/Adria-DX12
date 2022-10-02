#pragma once
#include <memory>
#include "GUI.h"
#include "EditorEvents.h"
#include "../Core/Engine.h"
#include "entt/entity/registry.hpp"
#include "../Rendering/RendererSettings.h"
#include "../Rendering/ViewportData.h"
#include "../Graphics/ProfilerSettings.h"
#include "../ImGui/imgui_internal.h"
#include "../ImGui/ImGuizmo.h"


namespace adria
{
	struct ImGuiLogger;
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
			Flag_RTDebug,
			Flag_Renderer,
			Flag_Ocean,
			Flag_Decal,
			Flag_Particles,
			Flag_Sky,
			Flag_AddEntities,
			Flag_Count
		};
	public:
		explicit Editor(EditorInit const& init);
		~Editor();
		void HandleWindowMessage(WindowMessage const& msg_data);
		void Run();

	private:
		std::unique_ptr<Engine> engine;
		std::unique_ptr<GUI> gui;
		std::unique_ptr<ImGuiLogger> editor_log;
		entt::entity selected_entity = entt::null;
		bool gizmo_enabled = false;
		bool scene_focused = false;
		ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;
		RendererSettings renderer_settings{};
		ProfilerSettings profiler_settings{};
		ViewportData viewport_data;
		EditorEvents editor_events;
		bool reload_shaders = false;
		std::queue<AABB*> aabb_updates;
		std::array<bool, Flag_Count> window_flags = { false };
	private:
		void SetStyle();
		void HandleInput();
		void MenuBar();
		void OceanSettings();
		void ParticleSettings();
		void DecalSettings();
		void SkySettings();
		void AddEntities();
		void ListEntities();
		void Properties();
		void Camera();
		void Scene();
		void Log();
		void RendererSettings();
		void Profiling();
		void ShaderHotReload();
		void RayTracingDebug();
	};
}

