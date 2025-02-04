#pragma once
#include "Rendering/ViewportData.h"
#include "Rendering/SceneConfig.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	struct WindowEventInfo;
	class Window;
	struct SceneConfig;
	class GfxDevice;
	class Renderer;
	class SceneLoader;
	struct EditorEvents;
	class ImGuiManager;
	class Camera;

	class Engine
	{
		friend class Editor;

	public:
		Engine(Window* window, std::string const& scene_file);
		ADRIA_NONCOPYABLE_NONMOVABLE(Engine)
		~Engine();

		void OnWindowEvent(WindowEventInfo const& msg_data);
		void Run();

	private:
		Window* window = nullptr;
		entt::registry reg;
		std::unique_ptr<Camera> camera;
		std::unique_ptr<GfxDevice> gfx;
		std::unique_ptr<Renderer> renderer;
		std::unique_ptr<SceneLoader> scene_loader;
		ViewportData viewport_data;
		std::optional<SceneConfig> scene_request;

	private:
		void InitializeScene(SceneConfig const&);
		void ProcessCVarIniFile(std::string const&);

		void NewSceneRequest(SceneConfig const& scene_cfg)
		{
			scene_request = scene_cfg;
		}
		void HandleSceneRequest();

		void Update(Float dt);
		void Render();

		void SetViewportData(ViewportData*);
		void RegisterEditorEventCallbacks(EditorEvents&);
	};
}