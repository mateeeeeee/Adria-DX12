#include "tracy/Tracy.hpp"
#include "Engine.h"
#include "Window.h"
#include "Input.h"
#include "Paths.h"
#include "CommandLineOptions.h"
#include "ConsoleManager.h"
#include "Logging/Logger.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Rendering/Renderer.h"
#include "Rendering/SceneConfig.h"
#include "Rendering/ShaderManager.h"
#include "Utilities/ThreadPool.h"
#include "Utilities/Random.h"
#include "Utilities/Timer.h"
#include "Utilities/StringUtil.h"
#include "Editor/EditorEvents.h"


namespace adria
{
	Engine::Engine(Window* window, std::string const& scene_file) : window{ window }, viewport_data{}
	{
		g_ThreadPool.Initialize();
		GfxShaderCompiler::Initialize();
		gfx = std::make_unique<GfxDevice>(window);
		ShaderManager::Initialize();
		g_TextureManager.Initialize(gfx.get());
		renderer = std::make_unique<Renderer>(reg, gfx.get(), window->Width(), window->Height());
		scene_loader = std::make_unique<SceneLoader>(reg, gfx.get());

		InputEvents& input_events = g_Input.GetInputEvents();
		input_events.window_resized_event.AddMember(&GfxDevice::OnResize, *gfx);
		input_events.window_resized_event.AddMember(&Renderer::OnResize, *renderer);
		input_events.right_mouse_clicked.AddMember(&Renderer::OnRightMouseClicked, *renderer);
		input_events.f6_pressed_event.AddMember(&Renderer::OnTakeScreenshot, *renderer);
		input_events.f5_pressed_event.AddStatic(ShaderManager::CheckIfShadersHaveChanged);

		SceneConfig scene_config{};
		if (ParseSceneConfig(scene_file, scene_config))
		{
			ProcessCVarIniFile(scene_config.ini_file);
			InitializeScene(scene_config);
		}
		else 
		{
			window->Quit(1);
			return;
		}

		input_events.window_resized_event.AddMember(&Camera::OnResize, *camera);
		input_events.scroll_mouse_event.AddMember(&Camera::Zoom, *camera);
	}

	Engine::~Engine()
	{
		g_TextureManager.Destroy();
		ShaderManager::Destroy();
		GfxShaderCompiler::Destroy();
		g_ThreadPool.Destroy();
	}

	void Engine::OnWindowEvent(WindowEventInfo const& msg_data)
	{
		g_Input.OnWindowEvent(msg_data);
	}

	void Engine::Run()
	{
		ZoneScopedN("Engine::Run");
		static Timer timer;
		Float const dt = timer.MarkInSeconds();
		g_Input.Tick();
		Update(dt);
		Render();
		FrameMarkNamed("EngineFrame");
	}

	void Engine::HandleSceneRequest()
	{
		if (scene_request)
		{
			gfx->WaitForGPU();
			g_TextureManager.Clear();
			reg.clear();
			ProcessCVarIniFile(scene_request->ini_file);
			gfx->SetRenderingNotStarted();
			InitializeScene(*scene_request);
			scene_request = std::nullopt;
		}
	}

	void Engine::Update(Float dt)
	{
		ZoneScopedN("Engine::Update");
		HandleSceneRequest();
		camera->Update(dt);
		renderer->NewFrame(camera.get());
		renderer->Update(dt);
		gfx->Update();
		
	}
	void Engine::Render()
	{
		ZoneScopedN("Engine::Render");
		gfx->BeginFrame();
		renderer->Render();
		gfx->EndFrame();
	}

	void Engine::SetViewportData(ViewportData* _viewport_data)
	{
		if (_viewport_data)
		{
			viewport_data = *_viewport_data;
		}
		else
		{
			viewport_data.scene_viewport_focused = true;
			viewport_data.mouse_position_x = g_Input.GetMousePositionX();
			viewport_data.mouse_position_y = g_Input.GetMousePositionY();

			viewport_data.scene_viewport_pos_x = static_cast<Float>(window->PositionX());
			viewport_data.scene_viewport_pos_y = static_cast<Float>(window->PositionY());
			viewport_data.scene_viewport_size_x = static_cast<Float>(window->Width());
			viewport_data.scene_viewport_size_y = static_cast<Float>(window->Height());
		}
		renderer->SetViewportData(viewport_data);
	}

	void Engine::RegisterEditorEventCallbacks(EditorEvents& events)
	{
		events.take_screenshot_event.AddMember(&Renderer::OnTakeScreenshot, *renderer);
		events.light_changed_event.AddMember(&Renderer::OnLightChanged, *renderer);
	}

	void Engine::InitializeScene(SceneConfig const& config)
	{
		auto cmd_list = gfx->GetLatestCommandList(GfxCommandListType::Graphics);
		cmd_list->Begin();

		camera = std::make_unique<Camera>(config.camera_params);
		camera->SetAspectRatio((Float)window->Width() / window->Height());
		scene_loader->LoadSkybox(config.skybox_params);

		for (auto const& model : config.scene_models) scene_loader->LoadModel(model);
		for (auto const& light : config.scene_lights) scene_loader->LoadLight(light);

		auto ray_tracing_view = reg.view<Mesh, RayTracing>();
		for (auto entity : reg.view<Mesh, RayTracing>())
		{
			auto const& mesh = ray_tracing_view.get<Mesh>(entity);
			GfxBuffer* buffer = g_GeometryBufferCache.GetGeometryBuffer(mesh.geometry_buffer_handle);
			cmd_list->BufferBarrier(*buffer, GfxResourceState::CopyDst, GfxResourceState::AllSRV);
		}

		renderer->OnSceneInitialized();
		cmd_list->End();
		cmd_list->Submit();
		gfx->WaitForGPU();
	}

	void Engine::ProcessCVarIniFile(std::string const& ini_file)
	{
		std::string cvar_ini_path = paths::IniDir + ini_file;
		std::ifstream cvar_ini_file(cvar_ini_path);
		if (!cvar_ini_file.is_open()) 
		{
			return;
		}
		std::string line;
		while (std::getline(cvar_ini_file, line))
		{
			if (line.empty() || line[0] == '#') continue;
			g_ConsoleManager.ProcessInput(line);
		}
	}
}
