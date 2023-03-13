#include "Engine.h"
#include "Window.h"
#include "../Editor/EditorEvents.h"
#include "../Tasks/TaskSystem.h"
#include "../Math/Constants.h"
#include "../Logging/Logger.h"
#include "../Input/Input.h"
#include "../Graphics/GfxDevice.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/Camera.h"
#include "../Rendering/EntityLoader.h"
#include "../Rendering/ShaderCache.h"
#include "../Rendering/PSOCache.h"
#include "../Utilities/Random.h"
#include "../Utilities/Timer.h"
#include "../Utilities/JsonUtil.h"
#include "../Utilities/StringUtil.h"
#include "../Utilities/FilesUtil.h"

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

	namespace
	{
		std::optional<SceneConfig> ParseSceneConfig(std::string const& scene_file)
		{
			SceneConfig config{};
			json models, lights, camera, skybox;
			try
			{
				JsonParams scene_params = json::parse(std::ifstream(scene_file));
				models = scene_params.FindJsonArray("models");
				lights = scene_params.FindJsonArray("lights");
				camera = scene_params.FindJson("camera");
				skybox = scene_params.FindJson("skybox");
			}
			catch (json::parse_error const& e)
			{
				ADRIA_LOG(ERROR, "JSON Parse error: %s! ", e.what());
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
				XMMATRIX translation = XMMatrixTranslation(position[0], position[1], position[2]);

				float angles[3] = { 0.0f, 0.0f, 0.0f };
				model_params.FindArray("rotation", angles);
				std::transform(std::begin(angles), std::end(angles), std::begin(angles), XMConvertToRadians);
				XMMATRIX rotation = XMMatrixRotationX(angles[0]) * XMMatrixRotationY(angles[1]) * XMMatrixRotationZ(angles[2]);

				float scale_factors[3] = { 1.0f, 1.0f, 1.0f };
				model_params.FindArray("scale", scale_factors);
				XMMATRIX scale = XMMatrixScaling(scale_factors[0], scale_factors[1], scale_factors[2]);
				XMMATRIX transform = rotation * scale * translation;

				bool used_for_ray_tracing = model_params.FindOr<bool>("ray_tracing", true);
				config.scene_models.emplace_back(path, tex_path, transform, used_for_ray_tracing);
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
				light.light_data.position = XMVectorSet(position[0], position[1], position[2], 1.0f);

				float direction[3] = { 0.0f, -1.0f, 0.0f };
				light_params.FindArray("direction", direction);
				light.light_data.direction = XMVectorSet(direction[0], direction[1], direction[2], 0.0f);

				float color[3] = { 1.0f, 1.0f, 1.0f };
				light_params.FindArray("color", color);
				light.light_data.color = XMVectorSet(color[0], color[1], color[2], 1.0f);

				light.light_data.energy = light_params.FindOr<float>("energy", 1.0f);
				light.light_data.range = light_params.FindOr<float>("range", 100.0f);

				light.light_data.outer_cosine = std::cos(XMConvertToRadians(light_params.FindOr<float>("outer_angle", 45.0f)));
				light.light_data.inner_cosine = std::cos(XMConvertToRadians(light_params.FindOr<float>("outer_angle", 22.5f)));

				light.light_data.casts_shadows = light_params.FindOr<bool>("shadows", true);
				light.light_data.use_cascades = light_params.FindOr<bool>("cascades", false);
				light.light_data.ray_traced_shadows = light_params.FindOr<bool>("rts", false);

				light.light_data.active = light_params.FindOr<bool>("active", true);
				light.light_data.volumetric = light_params.FindOr<bool>("volumetric", false);
				light.light_data.volumetric_strength = light_params.FindOr<float>("volumetric_strength", 0.3f);

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
				light.light_texture = ToWideString(light_params.FindOr<std::string>("texture", ""));
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
			config.camera_params.position = XMFLOAT3(position);

			float look_at[3] = { 0.0f, 0.0f, 10.0f };
			camera_params.FindArray("look_at", look_at);
			config.camera_params.look_at = XMFLOAT3(look_at);

			JsonParams skybox_params(skybox);
			std::vector<std::string> skybox_textures;

			std::string cubemap[1];
			if (skybox_params.FindArray("texture", cubemap))
			{
				config.skybox_params.cubemap = ToWideString(cubemap[0]);
			}
			else
			{
				std::string cubemap[6];
				if (skybox_params.FindArray("texture", cubemap))
				{
					std::wstring w_cubemap[6];
					std::transform(std::begin(cubemap), std::end(cubemap), std::begin(w_cubemap), [](std::string const& s) {
						return ToWideString(s); });
					config.skybox_params.cubemap_textures = std::to_array(w_cubemap);
				}
				else
				{
					ADRIA_LOG(WARNING, "Skybox texture not found or is incorrectly specified!  \
										Size of texture array has to be either 1 or 6! Fallback to the default one...");
					config.skybox_params.cubemap = L"Resources/Textures/Skybox/sunsetcube1024.dds";
				}
			}
			bool used_for_ray_tracing = skybox_params.FindOr<bool>("ray_tracing", true);
			config.skybox_params.used_in_rt = used_for_ray_tracing;

			return config;
		}
	}

	Engine::Engine(EngineInit const& init) : vsync{ init.vsync }
	{
		TaskSystem::Initialize();
		GfxShaderCompiler::Initialize();
		ShaderCache::Initialize();
		gfx = std::make_unique<GfxDevice>(GfxOptions{.debug_layer = init.debug_layer,
															   .dred = init.dred,
															   .gpu_validation = init.gpu_validation, .pix = init.pix });
		PSOCache::Initialize(gfx.get());
		TextureManager::Get().Initialize(gfx.get(), 1000);
		renderer = std::make_unique<Renderer>(reg, gfx.get(), Window::Width(), Window::Height());
		entity_loader = std::make_unique<EntityLoader>(reg, gfx.get());

		InputEvents& input_events = Input::GetInstance().GetInputEvents();
		input_events.window_resized_event.AddMember(&GfxDevice::ResizeBackbuffer, *gfx);
		input_events.window_resized_event.AddMember(&Renderer::OnResize, *renderer);
		input_events.right_mouse_clicked.AddMember(&Renderer::OnRightMouseClicked, *renderer);
		std::ignore = input_events.f5_pressed_event.Add(ShaderCache::CheckIfShadersHaveChanged);

		std::optional<SceneConfig> scene_config = ParseSceneConfig(init.scene_file);
		if (scene_config.has_value()) InitializeScene(scene_config.value());
		else Window::Quit(1);

		input_events.window_resized_event.AddMember(&Camera::OnResize, *camera);
		input_events.scroll_mouse_event.AddMember(&Camera::Zoom, *camera);
	}

	Engine::~Engine()
	{
		TextureManager::Get().Destroy();
		PSOCache::Destroy();
		ShaderCache::Destroy();
		GfxShaderCompiler::Destroy();
		TaskSystem::Destroy();
	}

	void Engine::HandleWindowMessage(WindowMessage const& msg_data)
	{
		Input::GetInstance().HandleWindowMessage(msg_data);
	}

	void Engine::Run(RendererSettings const& settings)
	{
		static AdriaTimer timer;
		float const dt = timer.MarkInSeconds();
		Input::GetInstance().NewFrame();
		if (true || Window::IsActive()) //crash when window is hidden, temp fix
		{	
			Update(dt);
			Render(settings);
		}
	}

	void Engine::Present()
	{
		gfx->SwapBuffers(vsync);
	}

	void Engine::Update(float dt)
	{
		camera->Tick(dt);
		renderer->NewFrame(camera.get());
		renderer->Update(dt);
	}

	void Engine::Render(RendererSettings const& settings)
	{
		gfx->ClearBackbuffer();
		renderer->Render(settings);
	}

	void Engine::SetViewportData(std::optional<ViewportData> _viewport_data)
	{
		if (_viewport_data.has_value())
		{
			viewport_data = _viewport_data.value();
		}
		else
		{
			viewport_data.scene_viewport_focused = true;
			viewport_data.mouse_position_x = Input::GetInstance().GetMousePositionX();
			viewport_data.mouse_position_y = Input::GetInstance().GetMousePositionY();

			auto [pos_x, pos_y] = Window::Position();
			viewport_data.scene_viewport_pos_x = static_cast<float>(pos_x);
			viewport_data.scene_viewport_pos_y = static_cast<float>(pos_y);
			viewport_data.scene_viewport_size_x = static_cast<float>(Window::Width());
			viewport_data.scene_viewport_size_y = static_cast<float>(Window::Height());
		}
		renderer->SetViewportData(viewport_data);
	}

	void Engine::RegisterEditorEventCallbacks(EditorEvents& events)
	{
		
	}

	void Engine::InitializeScene(SceneConfig const& config)
	{
		gfx->ResetCommandList();

		const_cast<SceneConfig&>(config).camera_params.aspect_ratio = static_cast<float>(Window::Width()) / Window::Height();
		camera = std::make_unique<Camera>(config.camera_params);
		entity_loader->LoadSkybox(config.skybox_params);

		for (auto&& model : config.scene_models) entity_loader->ImportModel_GLTF(model);
		for (auto&& light : config.scene_lights) entity_loader->LoadLight(light);

		renderer->OnSceneInitialized();
		gfx->ExecuteCommandList();
		gfx->WaitForGPU();
	}
}

