#pragma once
#include <memory>
#include <optional>
#include "Rendering/ViewportData.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	struct WindowEventData;
	class Window;
	struct SceneConfig;
	class GfxDevice;
	class Renderer;
	class EntityLoader;
	struct EditorEvents;
	class GUI;
	class Camera;

	struct EngineInit
	{
		std::string scene_file = "scene.json";
		Window* window = nullptr;
		//gfx options
		bool vsync = false;
		bool debug_device = false;
		bool gpu_validation = false;
		bool dred = false;
		bool pix = false;
		bool aftermath = false;
	};

	class Engine
	{
		friend class Editor;

	public:
		explicit Engine(EngineInit const&);
		ADRIA_NONCOPYABLE_NONMOVABLE(Engine)
		~Engine();

		void OnWindowEvent(WindowEventData const& msg_data);
		void Run();

	private:
		Window* window = nullptr;
		entt::registry reg;
		std::unique_ptr<Camera> camera;
	
		std::unique_ptr<GfxDevice> gfx;
		std::unique_ptr<Renderer> renderer;
		std::unique_ptr<EntityLoader> entity_loader;

		ViewportData viewport_data;
		bool vsync;

	private:
	
		virtual void InitializeScene(SceneConfig const& config);
		virtual void Update(float dt);
		virtual void Render();
		void SetViewportData(std::optional<ViewportData> viewport_data);
		void RegisterEditorEventCallbacks(EditorEvents&);
	};
}