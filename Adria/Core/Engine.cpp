#include "tracy/Tracy.hpp"
#include "Engine.h"
#include "Window.h"
#include "Input.h"
#include "Paths.h"
#include "ConsoleManager.h"
#include "Logging/Logger.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Rendering/Renderer.h"
#include "Rendering/Camera.h"
#include "Rendering/EntityLoader.h"
#include "Rendering/ShaderManager.h"
#include "Utilities/ThreadPool.h"
#include "Utilities/Random.h"
#include "Utilities/Timer.h"
#include "Utilities/JsonUtil.h"
#include "Utilities/StringUtil.h"
#include "Utilities/FilesUtil.h"
#include "Math/Constants.h"
#include "Editor/EditorEvents.h"

using namespace DirectX;
using json = nlohmann::json;

namespace adria
{
	struct SceneConfig
	{
		std::vector<ModelParameters> scene_models;
		std::vector<LightParameters> scene_lights;
		SkyboxParameters skybox_params;
		CameraParameters camera_params;
	};
	static std::optional<SceneConfig> ParseSceneConfig(std::string const& scene_file)
		{
			SceneConfig config{};
			json models, lights, camera, skybox;
			try
			{
				JsonParams scene_params = json::parse(std::ifstream(paths::ScenesDir + scene_file));
				models = scene_params.FindJsonArray("models");
				lights = scene_params.FindJsonArray("lights");
				camera = scene_params.FindJson("camera");
				skybox = scene_params.FindJson("skybox");
			}
			catch (json::parse_error const& e)
			{
				ADRIA_LOG(ERROR, "Scene json parsing error: %s! ", e.what());
				return std::nullopt;
			}

			for (auto&& model_json : models)
			{
				JsonParams model_params(model_json);

				std::string path;
				if (!model_params.Find<std::string>("path", path))
				{
					ADRIA_LOG(WARNING, "Model doesn't have path field! Skipping this model...");
				}
				std::string tex_path = model_params.FindOr<std::string>("tex_path", GetParentPath(path) + "\\");

				float position[3] = { 0.0f, 0.0f, 0.0f };
				model_params.FindArray("translation", position);
				Matrix translation = XMMatrixTranslation(position[0], position[1], position[2]);

				float angles[3] = { 0.0f, 0.0f, 0.0f };
				model_params.FindArray("rotation", angles);
				std::transform(std::begin(angles), std::end(angles), std::begin(angles), XMConvertToRadians);
				Matrix rotation = XMMatrixRotationX(angles[0]) * XMMatrixRotationY(angles[1]) * XMMatrixRotationZ(angles[2]);

				float scale_factors[3] = { 1.0f, 1.0f, 1.0f };
				model_params.FindArray("scale", scale_factors);
				Matrix scale = XMMatrixScaling(scale_factors[0], scale_factors[1], scale_factors[2]);
				Matrix transform = rotation * scale * translation;

				bool triangle_ccw = true;
				model_params.Find<bool>("use_ccw", triangle_ccw);
				bool force_mask = false;
				model_params.Find<bool>("force_alpha_mask", force_mask);
				config.scene_models.emplace_back(path, tex_path, transform, triangle_ccw, force_mask);
			}

			for (auto&& light_json : lights)
			{
				JsonParams light_params(light_json);

				std::string type;
				if (!light_params.Find<std::string>("type", type))
				{
					ADRIA_LOG(WARNING, "Light doesn't have type field! Skipping this light...");
				}

				LightParameters light{};
				float position[3] = { 0.0f, 0.0f, 0.0f };
				light_params.FindArray("position", position);
				light.light_data.position = Vector4(position[0], position[1], position[2], 1.0f);

				float direction[3] = { 0.0f, -1.0f, 0.0f };
				if (light_params.FindArray("direction", direction))
				{
					light.light_data.direction = Vector4(direction[0], direction[1], direction[2], 0.0f);
				}
				else if (type == "directional")
				{
					float elevation, azimuth;
					if (light_params.Find<float>("elevation", elevation) && light_params.Find<float>("azimuth", azimuth))
					{
						light.light_data.direction = -ConvertElevationAndAzimuthToDirection(elevation, azimuth);
					}
				}

				float color[3] = { 1.0f, 1.0f, 1.0f };
				if (light_params.FindArray("color", color))
				{
					light.light_data.color = Vector4(color[0], color[1], color[2], 1.0f);
				}
				else if (type == "directional")
				{
					float temperature;
					if (light_params.Find<float>("temperature", temperature))
					{
						light.light_data.color = ConvertTemperatureToColor(temperature);
					}
				}

				light.light_data.intensity = light_params.FindOr<float>("intensity", 1.0f);
				light.light_data.range = light_params.FindOr<float>("range", 100.0f);

				light.light_data.outer_cosine = std::cos(XMConvertToRadians(light_params.FindOr<float>("outer_angle", 45.0f)));
				light.light_data.inner_cosine = std::cos(XMConvertToRadians(light_params.FindOr<float>("outer_angle", 22.5f)));

				light.light_data.casts_shadows = light_params.FindOr<bool>("shadows", true);
				light.light_data.use_cascades = light_params.FindOr<bool>("cascades", false);
				light.light_data.ray_traced_shadows = light_params.FindOr<bool>("rts", false);

				light.light_data.active = light_params.FindOr<bool>("active", true);
				light.light_data.volumetric = light_params.FindOr<bool>("volumetric", false);
				light.light_data.volumetric_strength = light_params.FindOr<float>("volumetric_strength", 0.004f);

				light.light_data.lens_flare = light_params.FindOr<bool>("lens_flare", false);
				light.light_data.god_rays = light_params.FindOr<bool>("god_rays", false);

				light.light_data.godrays_decay = light_params.FindOr<float>("godrays_decay", 0.825f);
				light.light_data.godrays_exposure = light_params.FindOr<float>("godrays_exposure", 2.0f);
				light.light_data.godrays_density = light_params.FindOr<float>("godrays_density", 0.975f);
				light.light_data.godrays_weight = light_params.FindOr<float>("godrays_weight", 0.25f);

				light.mesh_type = LightMesh::NoMesh;
				std::string mesh = light_params.FindOr<std::string>("mesh", "");
				if (mesh == "sphere")
				{
					light.mesh_type = LightMesh::Sphere;
				}
				else if (mesh == "quad")
				{
					light.mesh_type = LightMesh::Quad;
				}
				light.mesh_size = light_params.FindOr<uint32>("size", 100u);
				light.light_texture = light_params.FindOr<std::string>("texture", "");
				if (light.light_texture.has_value() && light.light_texture->empty()) light.light_texture = std::nullopt;

				if (type == "directional")
				{
					light.light_data.type = LightType::Directional;
				}
				else if (type == "point")
				{
					light.light_data.type = LightType::Point;
				}
				else if (type == "spot")
				{
					light.light_data.type = LightType::Spot;
				}
				else
				{
					ADRIA_LOG(WARNING, "Light has invalid type %s! Skipping this light...", type.c_str());
				}

				config.scene_lights.push_back(std::move(light));
			}

			JsonParams camera_params(camera);
			config.camera_params.near_plane = camera_params.FindOr<float>("near", 1.0f);
			config.camera_params.far_plane = camera_params.FindOr<float>("far", 3000.0f);
			config.camera_params.fov = XMConvertToRadians(camera_params.FindOr<float>("fov", 90.0f));
			config.camera_params.sensitivity = camera_params.FindOr<float>("sensitivity", 0.3f);
			config.camera_params.speed = camera_params.FindOr<float>("speed", 25.0f);

			float position[3] = { 0.0f, 0.0f, 0.0f };
			camera_params.FindArray("position", position);
			config.camera_params.position = Vector3(position);

			float look_at[3] = { 0.0f, 0.0f, 10.0f };
			camera_params.FindArray("look_at", look_at);
			config.camera_params.look_at = Vector3(look_at);

			JsonParams skybox_params(skybox);
			std::vector<std::string> skybox_textures;

			std::string cubemap[1];
			if (skybox_params.FindArray("texture", cubemap))
			{
				config.skybox_params.cubemap = cubemap[0];
			}
			else
			{
				std::string cubemap[6];
				if (skybox_params.FindArray("texture", cubemap))
				{
					config.skybox_params.cubemap_textures = std::to_array(cubemap);
				}
				else
				{
					ADRIA_LOG(WARNING, "Skybox texture not found or is incorrectly specified!  \
										Size of texture array has to be either 1 or 6! Fallback to the default one...");
					config.skybox_params.cubemap = paths::TexturesDir + "Skybox/sunsetcube1024.dds";
				}
			}
			bool used_for_ray_tracing = skybox_params.FindOr<bool>("ray_tracing", true);

			return config;
		}


	Engine::Engine(EngineInit const& init) : window { init.window }
	{
		g_ThreadPool.Initialize();
		GfxShaderCompiler::Initialize();
		gfx = std::make_unique<GfxDevice>(window, init.gfx_options);
		ShaderManager::Initialize();
		g_TextureManager.Initialize(gfx.get(), 1000);
		renderer = std::make_unique<Renderer>(reg, gfx.get(), window->Width(), window->Height());
		entity_loader = std::make_unique<EntityLoader>(reg, gfx.get());

		InputEvents& input_events = g_Input.GetInputEvents();
		input_events.window_resized_event.AddMember(&GfxDevice::OnResize, *gfx);
		input_events.window_resized_event.AddMember(&Renderer::OnResize, *renderer);
		input_events.right_mouse_clicked.AddMember(&Renderer::OnRightMouseClicked, *renderer);
		input_events.f6_pressed_event.AddMember(&Renderer::OnTakeScreenshot, *renderer);
		std::ignore = input_events.f5_pressed_event.AddStatic(ShaderManager::CheckIfShadersHaveChanged);

		ProcessCVarIniFile();
		std::optional<SceneConfig> scene_config = ParseSceneConfig(init.scene_file);
		if (scene_config.has_value()) InitializeScene(scene_config.value());
		else window->Quit(1);

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

	void Engine::OnWindowEvent(WindowEventData const& msg_data)
	{
		g_Input.OnWindowEvent(msg_data);
	}

	void Engine::Run()
	{
		FrameMarkNamed("EngineFrame");
		static Timer timer;
		float const dt = timer.MarkInSeconds();
		g_Input.Tick();
		Update(dt);
		Render();
	}

	void Engine::Update(float dt)
	{
		camera->Tick(dt);
		renderer->NewFrame(camera.get());
		renderer->Update(dt);
	}
	void Engine::Render()
	{
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

			viewport_data.scene_viewport_pos_x = static_cast<float>(window->PositionX());
			viewport_data.scene_viewport_pos_y = static_cast<float>(window->PositionY());
			viewport_data.scene_viewport_size_x = static_cast<float>(window->Width());
			viewport_data.scene_viewport_size_y = static_cast<float>(window->Height());
		}
		renderer->SetViewportData(viewport_data);
	}

	void Engine::RegisterEditorEventCallbacks(EditorEvents& events)
	{
		events.take_screenshot_event.AddMember(&Renderer::OnTakeScreenshot, *renderer);
	}

	void Engine::InitializeScene(SceneConfig const& config)
	{
		auto cmd_list = gfx->GetCommandList();
		cmd_list->Begin();

		const_cast<SceneConfig&>(config).camera_params.aspect_ratio = static_cast<float>(window->Width()) / window->Height();
		camera = std::make_unique<Camera>(config.camera_params);
		entity_loader->LoadSkybox(config.skybox_params);

		for (auto const& model : config.scene_models) entity_loader->ImportModel_GLTF(model);
		for (auto const& light : config.scene_lights) entity_loader->LoadLight(light);

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

	void Engine::ProcessCVarIniFile()
	{
		std::string cvar_ini_path = paths::IniDir + "cvars.ini";
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
