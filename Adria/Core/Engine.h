#pragma once
#include <memory>
#include "../Events/EventQueue.h"
#include "../Input/Input.h"
#include "../tecs/registry.h"
#include "../Rendering/CameraManager.h"
#include "../Rendering/RendererSettings.h"

namespace adria
{
	struct window_message_t;
	struct SceneConfig;
	class GraphicsCoreDX12;
	class Renderer;
	class EntityLoader;
	class GUI;

	struct engine_init_t
	{
		bool vsync = false;
		char const* scene_file = "scene.json";
	};

	
	class Engine
	{
		friend class Editor;

	public:
		explicit Engine(engine_init_t const&);
		Engine(Engine const&) = delete;
		Engine(Engine&&) = delete;
		Engine& operator=(Engine const&) = delete;
		Engine& operator=(Engine&&) = delete;
		~Engine();
	
		void HandleWindowMessage(window_message_t const& msg_data);
		void Run(RendererSettings const& settings, bool offscreen = false);
		void Present();

	private:
		bool vsync;
		EventQueue event_queue; 
		Input input;
		tecs::registry reg;
		CameraManager camera_manager;
	
		std::unique_ptr<GraphicsCoreDX12> gfx;
		std::unique_ptr<Renderer> renderer;
		std::unique_ptr<EntityLoader> entity_loader;
	private:
	
		virtual void InitializeScene(SceneConfig const& config);
	
		virtual void Update(float32 dt);
	
		virtual void Render(RendererSettings const& settings, bool offscreen);
	};
}