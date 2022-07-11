#pragma once
#include <memory>
#include <optional>
#include "../Events/EventQueue.h"
#include "../Input/Input.h"
#include "entt/entity/registry.hpp"
#include "../Rendering/CameraManager.h"
#include "../Rendering/RendererSettings.h"
#include "../Rendering/ViewportData.h"

namespace adria
{
	struct WindowMessage;
	struct SceneConfig;
	class GraphicsDevice;
	class Renderer;
	class ModelImporter;
	struct EditorEvents;
	class GUI;

	struct EngineInit
	{
		bool vsync = false;
		bool debug_layer = false;
		bool gpu_validation = false;
		bool dred = false;
		std::string scene_file = "scene.json";
	};

	class Engine
	{
		friend class Editor;

	public:
		explicit Engine(EngineInit const&);
		Engine(Engine const&) = delete;
		Engine(Engine&&) = delete;
		Engine& operator=(Engine const&) = delete;
		Engine& operator=(Engine&&) = delete;
		~Engine();
	
		void HandleWindowMessage(WindowMessage const& msg_data);
		void Run(RendererSettings const& settings);
		void Present();

	private:
		bool vsync;
		Input input;
		entt::registry reg;
		CameraManager camera_manager;
	
		std::unique_ptr<GraphicsDevice> gfx;
		std::unique_ptr<Renderer> renderer;
		std::unique_ptr<ModelImporter> entity_loader;

		ViewportData viewport_data;
	private:
	
		virtual void InitializeScene(SceneConfig const& config);
		virtual void Update(float32 dt);
		virtual void Render(RendererSettings const& settings);
		void SetViewportData(std::optional<ViewportData> viewport_data);
		void RegisterEditorEventCallbacks(EditorEvents&);
	};
}