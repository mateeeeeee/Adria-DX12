#pragma once
#include <memory>
#include "GUI.h"
#include "EditorEvents.h"
#include "GUICommand.h"
#include "../Core/Engine.h"
#include "../Rendering/RendererSettings.h"
#include "../Rendering/ViewportData.h"
#include "../Graphics/ProfilerSettings.h"
#include "../ImGui/imgui_internal.h"
#include "../ImGui/ImGuizmo.h"
#include "entt/entity/registry.hpp"

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
		static Editor& GetInstance()
		{
			static Editor editor;
			return editor;
		}
		void Init(EditorInit&& init);
		void Destroy();
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
		Editor();
		~Editor();

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

